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

#include "Chat.h"
#include "ObjectMgr.h"
#include "PathFinder.h"
#include "TargetedMovementGenerator.h"
#include "MovementGenerator.h"
#include "FollowerReference.h"
#include "Geometry/Vector3.h"

bool ChatHandler::HandleDeMorphCommand(char* )
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        target = m_session->GetPlayer();
    }

    else if (IsPlayer(target) && HasLowerSecurity((Player*)target))
    {
        return false;
    }

    target->DeMorph();

    return true;
}

bool ChatHandler::HandleModifyMorphCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 display_id = (uint32)atoi(args);

    CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(display_id);
    if (!displayEntry)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* target = getSelectedUnit();
    if (!target)
    {
        target = m_session->GetPlayer();
    }

    else if (IsPlayer(target) && HasLowerSecurity((Player*)target))
    {
        return false;
    }

    target->SetDisplayId(display_id);

    return true;
}

bool ChatHandler::HandleDamageCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!target->IsAlive())
    {
        return true;
    }

    int32 damage_int;
    if (!ExtractInt32(&args, damage_int))
    {
        return false;
    }

    if (damage_int <= 0)
    {
        return true;
    }

    uint32 damage = damage_int;

    Player* player = m_session ? m_session->GetPlayer() : nullptr;

    if (!*args)
    {
        if (player)
        {
            player->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
            if (target != player)
            {
                player->SendAttackStateUpdate(HITINFO_NORMALSWING2, target, SPELL_SCHOOL_MASK_NORMAL, damage, 0, 0, VICTIMSTATE_NORMAL, 0);
            }
        }
        else
        {

            target->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
        }
        return true;
    }

    uint32 school;
    if (!ExtractUInt32(&args, school))
    {
        return false;
    }

    if (school >= MAX_SPELL_SCHOOL)
    {
        return false;
    }

    SpellSchoolMask schoolmask = GetSchoolMask(school);

    if (player && (schoolmask & SPELL_SCHOOL_MASK_NORMAL))
    {
        damage = player->CalcArmorReducedDamage(target, damage);
    }

    if (!*args)
    {
        uint32 absorb = 0;
        uint32 resist = 0;

        if (player)
        {
            target->CalculateDamageAbsorbAndResist(player, schoolmask, SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);

            if (damage <= absorb + resist)
            {
                return true;
            }

            damage -= absorb + resist;

            player->DealDamageMods(target, damage, &absorb);
            player->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, schoolmask, nullptr, false);
            player->SendAttackStateUpdate(HITINFO_NORMALSWING2, target, schoolmask, damage, absorb, resist, VICTIMSTATE_NORMAL, 0);
        }
        else
        {

            target->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, schoolmask, nullptr, false);
        }
        return true;
    }

    uint32 spellid = ExtractSpellIdFromLink(&args);
    if (!spellid || !sSpellStore.LookupEntry(spellid))
    {
        return false;
    }

    if (player)
    {
        player->SpellNonMeleeDamageLog(target, spellid, damage);
    }
    else
    {

        SendSysMessage("Spell damage requires an in-game player.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleDieCommand(char* )
{
    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (IsPlayer(target))
    {
        if (HasLowerSecurity((Player*)target, 0, false))
        {
            return false;
        }
    }

    if (target->IsAlive())
    {
        if (m_session)
        {

            m_session->GetPlayer()->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
        }
        else
        {

            target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
        }
    }

    return true;
}

bool ChatHandler::HandleMovegensCommand(char* )
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_MOVEGENS_LIST, (IsPlayer(unit) ? "Player" : "Creature"), unit->GetGUIDLow());

    MotionMaster* mm = unit->GetMotionMaster();
    float x, y, z;
    mm->GetDestination(x, y, z);
    for (MotionMaster::const_iterator itr = mm->begin(); itr != mm->end(); ++itr)
    {
        switch ((*itr)->GetMovementGeneratorType())
        {
            case IDLE_MOTION_TYPE:          SendSysMessage(LANG_MOVEGENS_IDLE);          break;
            case RANDOM_MOTION_TYPE:        SendSysMessage(LANG_MOVEGENS_RANDOM);        break;
            case WAYPOINT_MOTION_TYPE:      SendSysMessage(LANG_MOVEGENS_WAYPOINT);      break;
            case CONFUSED_MOTION_TYPE:      SendSysMessage(LANG_MOVEGENS_CONFUSED);      break;

            case CHASE_MOTION_TYPE:
            {
                Unit* target;
                if (IsPlayer(unit))
                {
                    target = static_cast<ChaseMovementGenerator const*>(*itr)->GetTarget();
                }
                else
                {
                    target = static_cast<ChaseMovementGenerator const*>(*itr)->GetTarget();
                }

                if (!target)
                {
                    SendSysMessage(LANG_MOVEGENS_CHASE_NULL);
                }
                else if (IsPlayer(target))
                {
                    PSendSysMessage(LANG_MOVEGENS_CHASE_PLAYER, target->GetName(), target->GetGUIDLow());
                }
                else
                {
                    PSendSysMessage(LANG_MOVEGENS_CHASE_CREATURE, target->GetName(), target->GetGUIDLow());
                }
                break;
            }
            case FOLLOW_MOTION_TYPE:
            {
                Unit* target;
                if (IsPlayer(unit))
                {
                    target = static_cast<FollowMovementGenerator const*>(*itr)->GetTarget();
                }
                else
                {
                    target = static_cast<FollowMovementGenerator const*>(*itr)->GetTarget();
                }

                if (!target)
                {
                    SendSysMessage(LANG_MOVEGENS_FOLLOW_NULL);
                }
                else if (IsPlayer(target))
                {
                    PSendSysMessage(LANG_MOVEGENS_FOLLOW_PLAYER, target->GetName(), target->GetGUIDLow());
                }
                else
                {
                    PSendSysMessage(LANG_MOVEGENS_FOLLOW_CREATURE, target->GetName(), target->GetGUIDLow());
                }
                break;
            }
            case HOME_MOTION_TYPE:
                if (IsCreature(unit))
                {
                    PSendSysMessage(LANG_MOVEGENS_HOME_CREATURE, x, y, z);
                }
                else
                {
                    SendSysMessage(LANG_MOVEGENS_HOME_PLAYER);
                }
                break;
            case FLIGHT_MOTION_TYPE:   SendSysMessage(LANG_MOVEGENS_FLIGHT);  break;
            case POINT_MOTION_TYPE:
            {
                PSendSysMessage(LANG_MOVEGENS_POINT, x, y, z);
                break;
            }
            case FLEEING_MOTION_TYPE:  SendSysMessage(LANG_MOVEGENS_FEAR);    break;
            case DISTRACT_MOTION_TYPE: SendSysMessage(LANG_MOVEGENS_DISTRACT);  break;
            case EFFECT_MOTION_TYPE: SendSysMessage(LANG_MOVEGENS_EFFECT);  break;
            default:
                PSendSysMessage(LANG_MOVEGENS_UNKNOWN, (*itr)->GetMovementGeneratorType());
                break;
        }
    }
    return true;
}

bool ChatHandler::HandleSetViewCommand(char* )
{
    if (Unit* unit = getSelectedUnit())
    {
        m_session->GetPlayer()->GetCamera().SetView(unit);
    }
    else
    {
        PSendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}
