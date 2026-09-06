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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Retinue.h"

#include "Map.h"
#include "Pet.h"
#include "Totem.h"
#include "Unit.h"

void Retinue::AddGuardian(Pet& guardian)
{
    m_guardians.insert(guardian.GetObjectGuid());
}

void Retinue::RemoveGuardian(Pet& guardian)
{
    m_guardians.erase(guardian.GetObjectGuid());
}

void Retinue::RemoveGuardians()
{
    while (!m_guardians.empty())
    {
        ObjectGuid const guid = *m_guardians.begin();

        if (Pet* guardian = m_owner.GetMap()->GetPet(guid))
        {
            guardian->Unsummon(PET_SAVE_AS_DELETED, &m_owner);   // this can take the guid out itself
        }

        m_guardians.erase(guid);
    }
}

Pet* Retinue::GuardianOfEntry(uint32 entry) const
{
    for (ObjectGuid const& guid : m_guardians)
    {
        Pet* guardian = m_owner.GetMap()->GetPet(guid);
        if (guardian && guardian->GetEntry() == entry)
        {
            return guardian;
        }
    }

    return nullptr;
}

Totem* Retinue::TotemIn(TotemSlot slot) const
{
    if (!m_owner.IsInWorld() || !m_totems[slot])
    {
        return nullptr;
    }

    Creature* standing = m_owner.GetMap()->GetCreature(m_totems[slot]);
    return standing && standing->IsTotem() ? static_cast<Totem*>(standing) : nullptr;
}

Unit* Retinue::UnitIn(TotemSlot slot) const
{
    return TotemIn(slot);
}

void Retinue::PutTotem(TotemSlot slot, Totem& totem)
{
    m_totems[slot] = totem.GetObjectGuid();
}

void Retinue::TakeTotem(Totem& totem)
{
    for (int slot = 0; slot < MAX_TOTEM_SLOT; ++slot)
    {
        if (m_totems[slot] == totem.GetObjectGuid())
        {
            m_totems[slot].Clear();
            break;
        }
    }
}

void Retinue::UnsummonAllTotems()
{
    for (int slot = 0; slot < MAX_TOTEM_SLOT; ++slot)
    {
        if (Totem* totem = TotemIn(TotemSlot(slot)))
        {
            totem->UnSummon();
        }
    }
}
