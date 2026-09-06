/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */



#include "Utterance.h"
#include "GameObject.h"
#include "Kinds.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include "Geometry/Quat.h"

/**
 * @brief Updates game object state, timers, loot state, and AI.
 *
 * @param update_diff The elapsed AI update time in milliseconds.
 * @param p_time The elapsed world time step in milliseconds.
 */
void GameObject::Update(uint32 update_diff, uint32 p_time)
{
    if (GetObjectGuid().IsMOTransport())
    {
        return;
    }

    // THE MACHINE IS THE SAME FOR EVERY GAMEOBJECT: it is made ready, it stands
    // ready until its clock or somebody takes it, it is in use, and then it is
    // spent and put back. What each KIND does at those five moments is its own,
    // and is asked of its behaviour.
    switch (m_lootState)
    {
        case GO_NOT_READY:
        {
            m_behaviour->Arming();
            break;
        }

        case GO_READY:
        {
            if (m_spawn.Moment() > 0 && m_spawn.Moment() <= time(nullptr))
            {
                m_spawn.ChangesAt(0);
                ClearAllUsesData();

                if (m_behaviour->TimedOut() == GameObjectBehaviour::Tick::Stop)
                {
                    return;
                }

                // A thing that only ever goes away is gone; one that belongs to the
                // world comes back into it.
                if (!m_spawn.IsPermanent())
                {
                    SetLootState(GO_JUST_DEACTIVATED);

                    // Summoned by a spell rather than placed by the data: nobody owns
                    // the row it would go back to, so there is nothing to put back.
                    if (!HasStaticDBSpawnData())
                    {
                        if (Unit* owner = GetOwner())
                        {
                            owner->Conjured().RemoveObject(this, false);
                        }
                        Delete();
                    }
                    return;
                }

                GetMap()->Add(this);
            }

            if (isSpawned())
            {
                GameObjectBehaviour::Tick const verdict = m_behaviour->Standing();
                if (verdict == GameObjectBehaviour::Tick::Stop)
                {
                    return;
                }
                if (verdict == GameObjectBehaviour::Tick::Rest)
                {
                    break;
                }

                // USES ARE COUNTED ONLY WHERE THE TEMPLATE SAYS THEY ARE. A charge
                // count of zero is not "no uses left", it is "this one is never used
                // up", which is why nothing with a zero here is ever despawned.
                if (uint32 const charges = GetGOInfo()->GetCharges())
                {
                    auto* counted = Behaves<CountingBehaviour>();

                    if (counted && counted->Tally().Uses() >= charges)
                    {
                        counted->Tally().Forget();
                        SetLootState(GO_JUST_DEACTIVATED);
                    }
                }
            }
            break;
        }

        case GO_ACTIVATED:
        {
            m_behaviour->InUse(p_time);
            break;
        }

        case GO_JUST_DEACTIVATED:
        {
            if (m_behaviour->Spent() == GameObjectBehaviour::Tick::Stop)
            {
                return;
            }

            // Wild-summoned things are not put back, they are done with.
            if (!HasStaticDBSpawnData() && (!GetSpellId() || GetGOInfo()->GetDespawnPossibility() || GetGOInfo()->IsDespawnAtAction()))
            {
                if (Unit* owner = GetOwner())
                {
                    owner->Conjured().RemoveObject(this, false);
                }
                Delete();
                return;
            }

            // burning flags in some battlegrounds, if you find better condition, just add it
            if (GetGOInfo()->IsDespawnAtAction() || GetGoAnimProgress() > 0)
            {
                SendDespawnAnimation(*this);

                if (GetMap()->Instanceable())
                {
                    // In Instances GO_FLAG_LOCKED, GO_FLAG_INTERACT_COND or GO_FLAG_NO_INTERACT are not changed
                    uint32 currentLockOrInteractFlags = GetGoFlags() & (GO_FLAG_LOCKED | GO_FLAG_INTERACT_COND | GO_FLAG_NO_INTERACT);
                    SetAllGoFlags((GetGOInfo()->flags & ~(GO_FLAG_LOCKED | GO_FLAG_INTERACT_COND | GO_FLAG_NO_INTERACT)) | currentLockOrInteractFlags);
                }
                else
                {
                    SetAllGoFlags(GetGOInfo()->flags);
                }
            }

            loot.clear();
            Claim().StakedBy(nullptr);
            SetLootState(GO_READY);

            if (!m_spawn.Delay())
            {
                return;
            }

            // since pool system can fail to roll unspawned object, this one can remain spawned, so must set respawn nevertheless
            m_spawn.ChangesAt(m_spawn.IsPermanent() ? time(nullptr) + m_spawn.Delay() : 0);

            // if option not set then object will be saved at grid unload
            if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
            {
                SaveRespawnTime();
            }

            // if part of pool, let pool system schedule new spawn instead of just scheduling respawn
            if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
            {
                sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
            }

            // can be not in world at pool despawn
            if (IsInWorld())
            {
                UpdateObjectVisibility();
            }

            m_behaviour->Respawning();

            break;
        }
    }

    if (AI())
    {
        // do not allow the AI to be changed during update
        m_AI_locked = true;
        AI()->UpdateAI(update_diff);   // AI not react good at real update delays (while freeze in non-active part of map)
        m_AI_locked = false;
    }
}
