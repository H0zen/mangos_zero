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

#include "Recovery.h"

regen::Share regen::PowerTick(Powers which, float spirit, uint32 maxPower,
                              bool answering, bool spentRecently, Rates const& rates)
{
    Share share;

    switch (which)
    {
        case POWER_MANA:
            share.any = true;
            if (answering)
            {
                // spirit only feeds the bar once the hold after spending is over
                if (!spentRecently)
                {
                    share.amount = (spirit / 5.0f + 17.0f) * rates.mana;
                }
            }
            else
            {
                share.amount = maxPower / 3.0f;
            }
            break;

        case POWER_ENERGY:
            share.any = true;
            share.amount = 20 * rates.energy;
            break;

        case POWER_FOCUS:
            share.any = true;
            share.amount = 24 * rates.focus;
            break;

        default:
            break;
    }

    return share;
}

uint32 regen::HealthTick(float spirit, uint32 maxHealth, bool mastered, bool hasMana, float rate)
{
    uint32 back = 0;

    if (mastered)
    {
        // a charmed creature has no spirit of its own, so this can come to nothing
        back = uint32(spirit * (hasMana ? 0.25 : 0.80) * rate);
    }

    if (back == 0)
    {
        back = maxHealth / 3;
    }

    return back;
}
