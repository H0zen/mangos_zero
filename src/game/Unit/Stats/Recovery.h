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

#pragma once

#include "Platform/Define.h"
#include "SharedDefines.h"

class Recovery
{
    public:

        static constexpr uint32 HOLD = 5000;

        void Run(uint32 elapsed)
        {
            m_next = m_next <= elapsed ? 0 : m_next - elapsed;
        }

        bool Due() const { return m_next == 0; }

        void NextIn(uint32 period) { m_next = period; }
        uint32 Left() const { return m_next; }

        void ManaSpent() { m_hold = HOLD; }

        void RunHold(uint32 elapsed)
        {
            m_hold = m_hold <= elapsed ? 0 : m_hold - elapsed;
        }

        bool HoldingBack() const { return m_hold != 0; }

    private:
        uint32 m_next = 0;
        uint32 m_hold = 0;
};

namespace regen
{

    struct Rates
    {
        float mana = 1.0f;
        float energy = 1.0f;
        float focus = 1.0f;
        float health = 1.0f;
    };

    struct Share
    {
        bool any = false;
        float amount = 0.0f;
    };

    Share PowerTick(Powers which, float spirit, uint32 maxPower,
                    bool answering, bool spentRecently, Rates const& rates);

    uint32 HealthTick(float spirit, uint32 maxHealth, bool mastered, bool hasMana, float rate);
}
