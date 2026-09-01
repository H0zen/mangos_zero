/**
 * ScriptDev3 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2014-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * ScriptData
 * SDName:      Guards
 * SD%Complete: 100
 * SDComment:   Zone guards
 * SDCategory:  Guards
 * EndScriptData
 */

/**
 * ContentData
 * guard_bluffwatcher
 * guard_contested
 * guard_darnassus
 * guard_dunmorogh
 * guard_durotar
 * guard_elwynnforest
 * guard_ironforge
 * guard_mulgore
 * guard_orgrimmar
 * guard_stormwind
 * guard_teldrassil
 * guard_tirisfal
 * guard_undercity
 * EndContentData
 */

#include "precompiled.h"
#include "guard_ai.h"

struct guard_generic : public CreatureScript
{
    guard_generic() : CreatureScript("guard_generic") {}

    CreatureAI* GetAI(Creature *pCreature) override
    {
        return new guardAI(pCreature);
    }
};

struct guard_orgrimmar : public CreatureScript
{
    guard_orgrimmar() : CreatureScript("guard_orgrimmar") {}

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new guardAI_orgrimmar(pCreature);
    }
};

struct guard_stormwind : public CreatureScript
{
    guard_stormwind() : CreatureScript("guard_stormwind") {}

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new guardAI_stormwind(pCreature);
    }
};

struct guard_ironforge : public CreatureScript
{
    guard_ironforge() : CreatureScript("guard_ironforge") {}

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new guardAI_ironforge(pCreature);
    }
};

void AddSC_guards()
{
    Script* s;
    s = new guard_generic();
    s->RegisterSelf();
    s = new guard_orgrimmar();
    s->RegisterSelf();
    s = new guard_stormwind();
    s->RegisterSelf();
    s = new guard_ironforge();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_contested";
    //pNewScript->GetAI = &GetAI_guard_contested;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_darnassus";
    //pNewScript->GetAI = &GetAI_guard_darnassus;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_dunmorogh";
    //pNewScript->GetAI = &GetAI_guard_dunmorogh;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_durotar";
    //pNewScript->GetAI = &GetAI_guard_durotar;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_elwynnforest";
    //pNewScript->GetAI = &GetAI_guard_elwynnforest;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_ironforge";
    //pNewScript->GetAI = &GetAI_guard_ironforge;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_stormwind";
    //pNewScript->GetAI = &GetAI_guard_stormwind;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_teldrassil";
    //pNewScript->GetAI = &GetAI_guard_teldrassil;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_tirisfal";
    //pNewScript->GetAI = &GetAI_guard_tirisfal;
    //pNewScript->RegisterSelf();
}
