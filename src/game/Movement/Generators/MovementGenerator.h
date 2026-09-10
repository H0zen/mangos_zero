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

#pragma once

#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "MotionMaster.h"
#include "Timer.h"

class Unit;
class Creature;
class Player;

class MovementGenerator
{
    public:
        virtual ~MovementGenerator() = default;

        virtual void Initialize(Unit& owner) = 0;

        virtual void Finalize(Unit& owner) = 0;

        virtual void Interrupt(Unit& owner) = 0;

        virtual void Reset(Unit& owner) = 0;

        virtual bool Update(Unit& owner, uint32 diff) = 0;

        virtual MovementGeneratorType GetMovementGeneratorType() const = 0;

        virtual void unitSpeedChanged() {}

        virtual bool GetResetPosition(Unit& , float& , float& ,
                                      float& , float& ) const
        {
            return false;
        }

        virtual bool IsReachable() const { return true; }

        virtual bool IsRoutedLeg() const { return false; }

        bool IsActive(Unit& owner);
};

struct SelectableMovement : public FactoryHolder<MovementGenerator, MovementGeneratorType>
{

    SelectableMovement(MovementGeneratorType mgt) : FactoryHolder<MovementGenerator, MovementGeneratorType>(mgt) {}
};

template<class REAL_MOVEMENT>
struct MovementGeneratorFactory : public SelectableMovement
{

    MovementGeneratorFactory(MovementGeneratorType mgt) : SelectableMovement(mgt) {}

    MovementGenerator* Create(void* data) const override;
};

typedef FactoryHolder<MovementGenerator, MovementGeneratorType> MovementGeneratorCreator;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRegistry MovementGeneratorRegistry;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRepository MovementGeneratorRepository;
