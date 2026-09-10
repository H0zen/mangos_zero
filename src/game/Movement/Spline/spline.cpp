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

#include "Utilities/Errors.h"
#include <string>
#include "spline.h"
#include <sstream>
#include "Geometry/Matrix4.h"

namespace Movement
{

    SplineBase::EvaluationMethtod SplineBase::evaluators[SplineBase::ModesEnd] =
    {
        &SplineBase::EvaluateLinear,
        &SplineBase::EvaluateCatmullRom,
        &SplineBase::EvaluateBezier3,
        (EvaluationMethtod)& SplineBase::UninitializedSpline,
    };

    SplineBase::EvaluationMethtod SplineBase::derivative_evaluators[SplineBase::ModesEnd] =
    {
        &SplineBase::EvaluateDerivativeLinear,
        &SplineBase::EvaluateDerivativeCatmullRom,
        &SplineBase::EvaluateDerivativeBezier3,
        (EvaluationMethtod)& SplineBase::UninitializedSpline,
    };

    SplineBase::SegLenghtMethtod SplineBase::seglengths[SplineBase::ModesEnd] =
    {
        &SplineBase::SegLengthLinear,
        &SplineBase::SegLengthCatmullRom,
        &SplineBase::SegLengthBezier3,
        (SegLenghtMethtod)& SplineBase::UninitializedSpline,
    };

    SplineBase::InitMethtod SplineBase::initializers[SplineBase::ModesEnd] =
    {

        &SplineBase::InitCatmullRom,
        &SplineBase::InitCatmullRom,
        &SplineBase::InitBezier3,
        (InitMethtod)& SplineBase::UninitializedSpline,
    };

    using Geometry::Matrix4;

    static const Matrix4 s_catmullRomCoeffs(
        -0.5f, 1.5f, -1.5f, 0.5f,
        1.f, -2.5f, 2.f, -0.5f,
        -0.5f, 0.f,  0.5f, 0.f,
        0.f,  1.f,  0.f,  0.f);

    static const Matrix4 s_Bezier3Coeffs(
        -1.f,  3.f, -3.f, 1.f,
        3.f, -6.f,  3.f, 0.f,
        -3.f,  3.f,  0.f, 0.f,
        1.f,  0.f,  0.f, 0.f);

    inline static void C_Evaluate(const Vector3* vertice, float t, const Matrix4& matr, Vector3& result)
    {
        Vector4 tvec(t * t * t, t * t, t, 1.f);
        Vector4 weights(tvec * matr);

        result = vertice[0] * weights[0] + vertice[1] * weights[1] +
            vertice[2] * weights[2] + vertice[3] * weights[3];
    }

    inline static void C_Evaluate_Derivative(const Vector3* vertice, float t, const Matrix4& matr, Vector3& result)
    {
        Vector4 tvec(3.f * t * t, 2.f * t, 1.f, 0.f);
        Vector4 weights(tvec * matr);

        result = vertice[0] * weights[0] + vertice[1] * weights[1] +
            vertice[2] * weights[2] + vertice[3] * weights[3];
    }

    void SplineBase::EvaluateLinear(index_type index, float u, Vector3& result) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        result = points[index] + (points[index + 1] - points[index]) * u;
    }

    void SplineBase::EvaluateCatmullRom(index_type index, float t, Vector3& result) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        C_Evaluate(&points[index - 1], t, s_catmullRomCoeffs, result);
    }

    void SplineBase::EvaluateBezier3(index_type index, float t, Vector3& result) const
    {
        index *= 3u;
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        C_Evaluate(&points[index], t, s_Bezier3Coeffs, result);
    }

    void SplineBase::EvaluateDerivativeLinear(index_type index, float, Vector3& result) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        result = points[index + 1] - points[index];
    }

    void SplineBase::EvaluateDerivativeCatmullRom(index_type index, float t, Vector3& result) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        C_Evaluate_Derivative(&points[index - 1], t, s_catmullRomCoeffs, result);
    }

    void SplineBase::EvaluateDerivativeBezier3(index_type index, float t, Vector3& result) const
    {
        index *= 3u;
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        C_Evaluate_Derivative(&points[index], t, s_Bezier3Coeffs, result);
    }

    float SplineBase::SegLengthLinear(index_type index) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);
        return (points[index] - points[index + 1]).length();
    }

    float SplineBase::SegLengthCatmullRom(index_type index) const
    {
        MANGOS_ASSERT(index >= index_lo && index < index_hi);

        Vector3 curPos, nextPos;
        const Vector3* p = &points[index - 1];
        curPos = nextPos = p[1];

        index_type i = 1;
        double length = 0;
        while (i <= STEPS_PER_SEGMENT)
        {
            C_Evaluate(p, float(i) / float(STEPS_PER_SEGMENT), s_catmullRomCoeffs, nextPos);
            length += (nextPos - curPos).length();
            curPos = nextPos;
            ++i;
        }
        return length;
    }

    float SplineBase::SegLengthBezier3(index_type index) const
    {
        index *= 3u;
        MANGOS_ASSERT(index >= index_lo && index < index_hi);

        Vector3 curPos, nextPos;
        const Vector3* p = &points[index];

        C_Evaluate(p, 0.f, s_Bezier3Coeffs, nextPos);
        curPos = nextPos;

        index_type i = 1;
        double length = 0;
        while (i <= STEPS_PER_SEGMENT)
        {
            C_Evaluate(p, float(i) / float(STEPS_PER_SEGMENT), s_Bezier3Coeffs, nextPos);
            length += (nextPos - curPos).length();
            curPos = nextPos;
            ++i;
        }
        return length;
    }

    void SplineBase::init_spline(const Vector3* controls, index_type count, EvaluationMode m)
    {
        m_mode = m;
        cyclic = false;

        (this->*initializers[m_mode])(controls, count, cyclic, 0);
    }

    void SplineBase::init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point)
    {
        m_mode = m;
        cyclic = true;

        (this->*initializers[m_mode])(controls, count, cyclic, cyclic_point);
    }

    void SplineBase::InitLinear(const Vector3* controls, index_type count, bool cyclic, index_type cyclic_point)
    {
        MANGOS_ASSERT(count >= 2);
        const int real_size = count + 1;

        points.resize(real_size);

        memcpy(&points[0], controls, sizeof(Vector3) * count);

        if (cyclic)
        {
            points[count] = controls[cyclic_point];
        }
        else
        {
            points[count] = controls[count - 1];
        }

        index_lo = 0;
        index_hi = cyclic ? count : (count - 1);
    }

    void SplineBase::InitCatmullRom(const Vector3* controls, index_type count, bool cyclic, index_type cyclic_point)
    {
        const int real_size = count + (cyclic ? (1 + 2) : (1 + 1));

        points.resize(real_size);

        int lo_index = 1;
        int high_index = lo_index + count - 1;

        memcpy(&points[lo_index], controls, sizeof(Vector3) * count);

        if (cyclic)
        {
            if (cyclic_point == 0)
            {
                points[0] = controls[count - 1];
            }
            else
            {
                points[0] = controls[0].lerp(controls[1], -1);
            }

            points[high_index + 1] = controls[cyclic_point];
            points[high_index + 2] = controls[cyclic_point + 1];
        }
        else
        {
            points[0] = controls[0].lerp(controls[1], -1);
            points[high_index + 1] = controls[count - 1];
        }

        index_lo = lo_index;
        index_hi = high_index + (cyclic ? 1 : 0);
    }

    void SplineBase::InitBezier3(const Vector3* controls, index_type count, bool , index_type )
    {
        index_type c = count / 3u * 3u;
        index_type t = c / 3u;

        points.resize(c);
        memcpy(&points[0], controls, sizeof(Vector3) * c);

        index_lo = 0;
        index_hi = t - 1;

    }

    void SplineBase::clear()
    {
        index_lo = 0;
        index_hi = 0;
        points.clear();
    }

    std::string SplineBase::ToString() const
    {
        std::stringstream str;
        const char* mode_str[ModesEnd] = {"Linear", "CatmullRom", "Bezier3", "Uninitialized"};

        index_type count = this->points.size();
        str << "mode: " << mode_str[mode()] << std::endl;
        str << "points count: " << count << std::endl;
        for (index_type i = 0; i < count; ++i)
        {
            str << "point " << i << " : " << points[i].toString() << std::endl;
        }

        return str.str();
    }
}
