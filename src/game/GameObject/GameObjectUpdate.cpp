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

void GameObject::Update(uint32 update_diff, uint32 p_time)
{
    if ((GuidHigh(GetObjectGuid()) == HIGHGUID_MO_TRANSPORT))
    {
        return;
    }

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

                if (!m_spawn.IsPermanent())
                {
                    SetLootState(GO_JUST_DEACTIVATED);

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

            if (!HasStaticDBSpawnData() && (!GetSpellId() || GetGOInfo()->GetDespawnPossibility() || GetGOInfo()->IsDespawnAtAction()))
            {
                if (Unit* owner = GetOwner())
                {
                    owner->Conjured().RemoveObject(this, false);
                }
                Delete();
                return;
            }

            if (GetGOInfo()->IsDespawnAtAction() || GetGoAnimProgress() > 0)
            {
                SendDespawnAnimation(*this);

                if (GetMap()->Instanceable())
                {

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

            m_spawn.ChangesAt(m_spawn.IsPermanent() ? time(nullptr) + m_spawn.Delay() : 0);

            if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
            {
                SaveRespawnTime();
            }

            if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
            {
                sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
            }

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

        m_AI_locked = true;
        AI()->UpdateAI(update_diff);
        m_AI_locked = false;
    }
}
