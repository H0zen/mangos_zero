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

#include <iterator>
#include <random>
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"

void Spell::EffectScriptEffect(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->ID)
            {
                case 1509:
                {
                    if (IsPlayer(unitTarget))
                    {
                        ((Player*)unitTarget)->SetGameMaster(false);
                    }
                    break;
                }
                case 18139:
                {
                    if (IsPlayer(unitTarget))
                    {
                        ((Player*)unitTarget)->SetGameMaster(true);
                    }
                    break;
                }

                case 5249:
                {
                    if (unitTarget)
                    {
                        m_caster->CastSpell(unitTarget, 22856, true);
                        sLog.outString("EffectScriptEffect : %s target of spell 5249", unitTarget->GetName());
                    }
                    break;
                }
                case 8856:
                {
                    if (!itemTarget && !IsPlayer(m_caster))
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 5))
                    {
                        case 1:  spell_id = 8854; break;
                        default: spell_id = 8855; break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, nullptr);
                    return;
                }
                case 17512:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    unitTarget->HandleEmoteCommand(EMOTE_STATE_DANCE);

                    return;
                }
                case 22539:
                case 22972:
                case 22975:
                case 22976:
                case 22977:
                case 22978:
                case 22979:
                case 22980:
                case 22981:
                case 22982:
                case 22983:
                case 22984:
                case 22985:
                {
                    if (!unitTarget || !unitTarget->IsAlive())
                    {
                        return;
                    }

                    if (unitTarget->GetDummyAura(22683))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 22682, true);
                    return;
                }
                case 24194:
                case 24195:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    uint8 race = m_caster->getRace();
                    uint32 spellId = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 24194:
                            switch (race)
                            {
                                case RACE_HUMAN:            spellId = 24105; break;
                                case RACE_DWARF:            spellId = 24107; break;
                                case RACE_NIGHTELF:         spellId = 24108; break;
                                case RACE_GNOME:            spellId = 24106; break;
                            }
                            break;
                        case 24195:
                            switch (race)
                            {
                                case RACE_ORC:              spellId = 24104; break;
                                case RACE_UNDEAD:           spellId = 24103; break;
                                case RACE_TAUREN:           spellId = 24102; break;
                                case RACE_TROLL:            spellId = 24101; break;
                            }
                            break;
                    }

                    if (spellId)
                    {
                        m_caster->CastSpell(m_caster, spellId, true);
                    }

                    return;
                }
                case 24320:
                {
                    unitTarget->CastSpell(unitTarget, 24321, true, nullptr, nullptr, m_caster->GetObjectGuid());
                    return;
                }
                case 24324:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, unitTarget->HasAura(24321) ? 24323 : 24322, true);
                    return;
                }
                case 24590:
                    unitTarget->RemoveStacks(24575);
                    return;
                case 24714:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    if (roll_chance_i(14))
                    {
                        m_caster->CastSpell(m_caster, 24753, true);
                    }
                    else
                    {
                        m_caster->CastSpell(m_caster, 24720, true);
                    }

                    return;
                }
                case 24717:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, unitTarget->getGender() == GENDER_MALE ? 24708 : 24709, true);
                    return;
                }
                case 24718:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, unitTarget->getGender() == GENDER_MALE ? 24711 : 24710, true);
                    return;
                }
                case 24719:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, unitTarget->getGender() == GENDER_MALE ? 24712 : 24713, true);
                    return;
                }
                case 24720:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    uint32 spellId = 0;

                    switch (urand(0, 6))
                    {
                        case 0:
                            spellId = unitTarget->getGender() == GENDER_MALE ? 24708 : 24709;
                            break;
                        case 1:
                            spellId = unitTarget->getGender() == GENDER_MALE ? 24711 : 24710;
                            break;
                        case 2:
                            spellId = unitTarget->getGender() == GENDER_MALE ? 24712 : 24713;
                            break;
                        case 3:
                            spellId = 24723;
                            break;
                        case 4:
                            spellId = 24732;
                            break;
                        case 5:
                            spellId = unitTarget->getGender() == GENDER_MALE ? 24735 : 24736;
                            break;
                        case 6:
                            spellId = 24740;
                            break;
                    }

                    m_caster->CastSpell(unitTarget, spellId, true);
                    return;
                }
                case 24737:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, unitTarget->getGender() == GENDER_MALE ? 24735 : 24736, true);
                    return;
                }
                case 24751:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 24755, true);

                    unitTarget->CastSpell(unitTarget, roll_chance_i(50) ? 24714 : 24715, true);
                    return;
                }
                case 26004:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->HandleEmote(EMOTE_ONESHOT_CHEER);
                    return;
                }
                case 26137:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, urand(0, 1) ? 26009 : 26136, true);
                    return;
                }
                case 26218:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    uint32 spells[2] = {26206, 26207};

                    m_caster->CastSpell(unitTarget, spells[urand(0, 1)], true);
                    return;
                }
                case 26275:
                {
                    uint32 spells[4] = {26272, 26157, 26273, 26274};

                    for (int j = 0; j < 4; ++j)
                    {
                        if (unitTarget->HasAura(spells[j], EFFECT_INDEX_0))
                        {
                            return;
                        }
                    }

                    unitTarget->CastSpell(unitTarget, spells[urand(0, 3)], true);
                    return;
                }
                case 26465:
                    unitTarget->RemoveStacks(26464);
                    return;
                case 26656:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasOfType(SPELL_AURA_MOUNTED);

                    if (unitTarget->GetTerrain()->GetAreaId(unitTarget->Where().X(), unitTarget->Where().Y(), unitTarget->Where().Z()) == 3428)
                    {
                        unitTarget->CastSpell(unitTarget, 25863, false);
                    }
                    else
                    {
                        unitTarget->CastSpell(unitTarget, 26655, false);
                    }

                    return;
                }
                case 27687:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 27690, true);
                    unitTarget->CastSpell(unitTarget, 27691, true);
                    unitTarget->CastSpell(unitTarget, 27692, true);
                    unitTarget->CastSpell(unitTarget, 27693, true);
                    return;
                }
                case 27695:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 27696, true);
                    unitTarget->CastSpell(unitTarget, 27697, true);
                    unitTarget->CastSpell(unitTarget, 27698, true);
                    unitTarget->CastSpell(unitTarget, 27699, true);
                    return;
                }
                case 28352:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 28342, true);
                    return;
                }

                case 28374:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    int32 damage = unitTarget->GetHealth() - unitTarget->GetMaxHealth() * 0.05f;
                    if (damage > 0)
                    {
                        m_caster->CastCustomSpell(unitTarget, 28375, &damage, nullptr, nullptr, true);
                    }
                    return;
                }
                case 28560:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 28561, true, nullptr, nullptr, m_caster->GetObjectGuid());
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            switch (m_spellInfo->ID)
            {
                case  6201:
                case  6202:
                case  5699:
                case 11729:
                case 11730:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 itemtype;
                    uint32 rank = 0;
                    const auto mDummyAuras = unitTarget->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto* aura : mDummyAuras)
                    {
                        if (aura->GetId() == 18692)
                        {
                            rank = 1;
                            break;
                        }
                        else if (aura->GetId() == 18693)
                        {
                            rank = 2;
                            break;
                        }
                    }

                    static uint32 const itypes[5][3] =
                    {
                        { 5512, 19004, 19005},
                        { 5511, 19006, 19007},
                        { 5509, 19008, 19009},
                        { 5510, 19010, 19011},
                        { 9421, 19012, 19013}
                    };

                    switch (m_spellInfo->ID)
                    {
                        case  6201:
                            itemtype = itypes[0][rank]; break;
                        case  6202:
                            itemtype = itypes[1][rank]; break;
                        case  5699:
                            itemtype = itypes[2][rank]; break;
                        case 11729:
                            itemtype = itypes[3][rank]; break;
                        case 11730:
                            itemtype = itypes[4][rank]; break;
                        default:
                            return;
                    }
                    DoCreateItem(eff_idx, itemtype);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {

            if (m_spellInfo->SpellIconID == 70)
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }
                int32 heal = damage;
                int32 spellid = m_spellInfo->ID;
                m_caster->CastCustomSpell(unitTarget, 19968, &heal, &spellid, nullptr, true, nullptr, nullptr, 0, m_spellInfo);
            }

            else if (m_spellInfo->SpellIconID  == 242)
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }
                int32 heal = damage;
                int32 spellid = m_spellInfo->ID;
                m_caster->CastCustomSpell(unitTarget, 19993, &heal, &spellid, nullptr, true);
            }
            else if (m_spellInfo->SpellIconID == 205)
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                uint32 spellId2 = 0;

                const auto m_dummyAuras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (auto* auraOf : m_dummyAuras)
                {
                    SpellEntry const* spellInfo = auraOf->GetSpellProto();

                    if (!spellInfo || !IsSealSpell(auraOf->GetSpellProto()))
                    {
                        continue;
                    }

                    SpellEntry const* sealInfo = auraOf->GetSpellProto();
                    for (int32 eff = EFFECT_INDEX_0; eff < MAX_EFFECT_INDEX; ++eff)
                    {
                        uint32 val = sealInfo->CalculateSimpleValue(SpellEffectIndex(eff));
                        if (val > 10000)
                        {
                            spellId2 = val;
                            break;
                        }
                    }

                    if (spellId2 <= 1)
                    {
                        continue;
                    }

                    m_caster->RemoveAuras(auraOf->GetId());

                    break;
                }

                m_caster->CastSpell(unitTarget, spellId2, true);

                return;
            }
            break;
        }
    }

    if (!unitTarget)
    {
        return;
    }

    if (IsCreature(unitTarget) ||IsPlayer(unitTarget))
    {
        if (sScriptMgr.OnEffectScriptEffect(m_caster, m_spellInfo->ID, eff_idx, unitTarget, m_originalCasterGUID))
        {
            return;
        }
    }

    if (!ScriptMgr::CanSpellEffectStartDBScript(m_spellInfo, eff_idx))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectScriptEffect", m_spellInfo->ID);
    m_caster->GetMap()->Scripts().Start(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
