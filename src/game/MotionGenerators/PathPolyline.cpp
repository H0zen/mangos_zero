/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#include "PathPolyline.h"

#include <algorithm>
#include <cmath>

namespace PathPolyline
{
    bool CutToLength(PointsArray& points, float limit)
    {
        if (!(limit > 0.0f))
        {
            return false;
        }

        float walked = 0.0f;
        for (size_t i = 1; i < points.size(); ++i)
        {
            const Movement::Vector3 edge = points[i] - points[i - 1];
            const float length = edge.length();
            if (walked + length >= limit)
            {
                const bool cut = walked + length > limit || i + 1 < points.size();
                points[i] = points[i - 1] + edge * ((limit - walked) / length);
                points.resize(i + 1);
                return cut;
            }
            walked += length;
        }
        return false;
    }

    void Subdivide(PointsArray& points, float maxEdge)
    {
        if (!(maxEdge > 0.0f) || points.size() < 2)
        {
            return;
        }

        PointsArray out;
        out.reserve(points.size());
        out.push_back(points.front());
        for (size_t i = 1; i < points.size(); ++i)
        {
            const Movement::Vector3 from = points[i - 1];
            const Movement::Vector3 edge = points[i] - from;
            const float length = edge.length();
            const size_t pieces = std::isfinite(length) && length > maxEdge
                                      ? size_t(std::ceil(length / maxEdge))
                                      : 1;
            for (size_t piece = 1; piece < pieces; ++piece)
            {
                out.push_back(from + edge * (float(piece) / float(pieces)));
            }
            out.push_back(points[i]);
        }
        points.swap(out);
    }

    bool FitPackBox(PointsArray& points)
    {
        if (points.size() < 3)
        {
            return false;
        }

        Movement::Vector3 low = points.front();
        Movement::Vector3 high = points.front();
        for (size_t i = 1; i < points.size(); ++i)
        {
            const Movement::Vector3& point = points[i];
            low = Movement::Vector3(std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z));
            high = Movement::Vector3(std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z));
            if (high.x - low.x > PACK_BOX_XY || high.y - low.y > PACK_BOX_XY || high.z - low.z > PACK_BOX_Z)
            {
                const size_t keep = std::max<size_t>(i, 2);
                if (keep >= points.size())
                {
                    return false;
                }
                points.resize(keep);
                return true;
            }
        }
        return false;
    }
}
