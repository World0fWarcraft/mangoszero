/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#include "MoveSpline.h"
#include <sstream>
#include "Log.h"
#include "Unit.h"

namespace Movement
{
    extern float computeFallTime(float path_length, bool isSafeFall);
    extern float computeFallElevation(float time_passed, bool isSafeFall, float start_velocy);
    extern float computeFallElevation(float time_passed);

    /**
     * @brief Computes the current position on the spline.
     * @return The computed location.
     */
    Location MoveSpline::ComputePosition() const
    {
        MANGOS_ASSERT(Initialized());

        float u = 1.f;
        int32 seg_time = spline.length(point_Idx, point_Idx + 1);
        if (seg_time > 0)
        {
            u = (time_passed - spline.length(point_Idx)) / (float)seg_time;
        }
        Location c;
        spline.evaluate_percent(point_Idx, u, c);

        if (splineflags.falling)
        {
            computeFallElevation(c.z);
        }

        if (splineflags.done && splineflags.isFacing())
        {
            if (splineflags.final_angle)
            {
                c.orientation = facing.angle;
            }
            else if (splineflags.final_point)
            {
                c.orientation = atan2(facing.f.y - c.y, facing.f.x - c.x);
            }
            // nothing to do for MoveSplineFlag::Final_Target flag
        }
        else
        {
            Vector3 hermite;
            spline.evaluate_derivative(point_Idx, u, hermite);
            c.orientation = atan2(hermite.y, hermite.x);
        }
        c.orientation = G3D::wrap(c.orientation, 0.f, (float)G3D::twoPi());
        return c;
    }

    /**
     * @brief Computes the elevation during a fall.
     * @param el The elevation to be computed.
     */
    void MoveSpline::computeFallElevation(float& el) const
    {
        float z_now = spline.getPoint(spline.first()).z - Movement::computeFallElevation(MSToSec(time_passed));
        float final_z = FinalDestination().z;
        if (z_now < final_z)
        {
            el = final_z;
        }
        else
        {
            el = z_now;
        }
    }

    /**
     * @brief Computes the duration of the movement.
     * @param length The length of the path.
     * @param velocity The velocity of the movement.
     * @return The computed duration in milliseconds.
     */
    inline uint32 computeDuration(float length, float velocity)
    {
        return SecToMS(length / velocity);
    }

    /**
     * @brief Struct for initializing fall parameters.
     */
    struct FallInitializer
    {
        FallInitializer(float _start_elevation) : start_elevation(_start_elevation) {}
        float start_elevation;
        inline int32 operator()(Spline<int32>& s, int32 i)
        {
            return Movement::computeFallTime(start_elevation - s.getPoint(i + 1).z, false) * 1000.f;
        }
    };

    enum
    {
        minimal_duration = 1,
    };

    /**
     * @brief Struct for initializing common parameters.
     */
    struct CommonInitializer
    {
        CommonInitializer(float _velocity) : velocityInv(1000.f / _velocity), time(minimal_duration) {}
        float velocityInv;
        int32 time;
        inline int32 operator()(Spline<int32>& s, int32 i)
        {
            time += (s.SegLength(i) * velocityInv);
            return time;
        }
    };

    /**
     * @brief Initializes the spline with the given arguments.
     * @param args The initialization arguments.
     */
    void MoveSpline::init_spline(const MoveSplineInitArgs& args)
    {
        const SplineBase::EvaluationMode modes[2] = {SplineBase::ModeLinear, SplineBase::ModeCatmullrom};
        if (args.flags.cyclic)
        {
            uint32 cyclic_point = 0;
            // MoveSplineFlag::Enter_Cycle support dropped
            // if (splineflags & SPLINEFLAG_ENTER_CYCLE)
            // cyclic_point = 1;   // shouldn't be modified, came from client
            spline.init_cyclic_spline(&args.path[0], args.path.size(), modes[args.flags.isSmooth()], cyclic_point);
        }
        else
        {
            spline.init_spline(&args.path[0], args.path.size(), modes[args.flags.isSmooth()]);
        }

        // init spline timestamps
        if (splineflags.falling)
        {
            FallInitializer init(spline.getPoint(spline.first()).z);
            spline.initLengths(init);
        }
        else
        {
            CommonInitializer init(args.velocity);
            spline.initLengths(init);
        }

        // TODO: what to do in such cases? problem is in input data (all points are at same coords)
        if (spline.length() < minimal_duration)
        {
            sLog.outError("MoveSpline::init_spline: zero length spline, wrong input data?");
            spline.set_length(spline.last(), spline.isCyclic() ? 1000 : 1);
        }
        point_Idx = spline.first();
    }

    /**
     * @brief Initializes the MoveSpline with the given arguments.
     * @param args The initialization arguments.
     */
    void MoveSpline::Initialize(const MoveSplineInitArgs& args)
    {
        splineflags = args.flags;
        facing = args.facing;
        m_Id = args.splineId;
        point_Idx_offset = args.path_Idx_offset;
        time_passed = 0;

        // detect Stop command
        if (splineflags.done)
        {
            spline.clear();
            return;
        }

        init_spline(args);
    }

    /**
     * @brief Default constructor for MoveSpline.
     */
    MoveSpline::MoveSpline() : m_Id(0), time_passed(0), point_Idx(0), point_Idx_offset(0)
    {
        splineflags.done = true;
    }

/// ============================================================================================

    /**
     * @brief Validates the MoveSpline initialization arguments.
     * @param unit The unit to validate against.
     * @return True if the arguments are valid, false otherwise.
     */
    bool MoveSplineInitArgs::Validate(Unit* unit) const
    {
#define CHECK(exp) \
    if (!(exp))\
    {\
        sLog.outError("MoveSplineInitArgs::Validate: expression '%s' failed for %s", #exp, unit->GetGuidStr().c_str());\
        return false;\
    }
        CHECK(path.size() > 1);
        CHECK(velocity > 0.f);
        // CHECK(_checkPathBounds());
        return true;
#undef CHECK
    }

// MONSTER_MOVE packet format limitation for not CatmullRom movement:
// each vertex offset packed into 11 bytes
    /**
     * @brief Checks the bounds of the path for non-CatmullRom movement.
     * @return True if the path bounds are valid, false otherwise.
     */
    bool MoveSplineInitArgs::_checkPathBounds() const
    {
        if (!(flags & MoveSplineFlag::Mask_CatmullRom) && path.size() > 2)
        {
            enum
            {
                MAX_OFFSET = (1 << 11) / 2,
            };
            Vector3 middle = (path.front() + path.back()) / 2;
            Vector3 offset;
            for (uint32 i = 1; i < path.size() - 1; ++i)
            {
                offset = path[i] - middle;
                if (fabs(offset.x) >= MAX_OFFSET || fabs(offset.y) >= MAX_OFFSET || fabs(offset.z) >= MAX_OFFSET)
                {
                    sLog.outError("MoveSplineInitArgs::_checkPathBounds check failed");
                    return false;
                }
            }
        }
        return true;
    }

/// ============================================================================================

    /**
     * @brief Updates the state of the MoveSpline.
     * @param ms_time_diff The time difference in milliseconds.
     * @return The result of the update.
     */
    MoveSpline::UpdateResult MoveSpline::_updateState(int32& ms_time_diff)
    {
        if (Finalized())
        {
            ms_time_diff = 0;
            return Result_Arrived;
        }

        UpdateResult result = Result_None;

        int32 minimal_diff = std::min(ms_time_diff, segment_time_elapsed());
        MANGOS_ASSERT(minimal_diff >= 0);
        time_passed += minimal_diff;
        ms_time_diff -= minimal_diff;

        if (time_passed >= next_timestamp())
        {
            ++point_Idx;
            if (point_Idx < spline.last())
            {
                result = Result_NextSegment;
            }
            else
            {
                if (spline.isCyclic())
                {
                    point_Idx = spline.first();
                    time_passed = time_passed % Duration();
                    result = Result_NextSegment;
                }
                else
                {
                    _Finalize();
                    ms_time_diff = 0;
                    result = Result_Arrived;
                }
            }
        }

        return result;
    }

    /**
     * @brief Converts the MoveSpline to a string representation.
     * @return The string representation of the MoveSpline.
     */
    std::string MoveSpline::ToString() const
    {
        std::stringstream str;
        str << "MoveSpline" << std::endl;
        str << "spline Id: " << GetId() << std::endl;
        str << "flags: " << splineflags.ToString() << std::endl;
        if (splineflags.final_angle)
        {
            str << "facing  angle: " << facing.angle;
        }
        else if (splineflags.final_target)
        {
            str << "facing target: " << facing.target;
        }
        else if (splineflags.final_point)
        {
            str << "facing  point: " << facing.f.x << " " << facing.f.y << " " << facing.f.z;
        }
        str << std::endl;
        str << "time passed: " << time_passed << std::endl;
        str << "total  time: " << Duration() << std::endl;
        str << "spline point Id: " << point_Idx << std::endl;
        str << "path  point  Id: " << currentPathIdx() << std::endl;
        str << spline.ToString();
        return str.str();
    }

    /**
     * @brief Finalizes the MoveSpline.
     */
    void MoveSpline::_Finalize()
    {
        splineflags.done = true;
        point_Idx = spline.last() - 1;
        time_passed = Duration();
    }

    /**
     * @brief Gets the current path index.
     * @return The current path index.
     */
    int32 MoveSpline::currentPathIdx() const
    {
        int32 point = point_Idx_offset + point_Idx - spline.first() + (int)Finalized();
        if (isCyclic())
        {
            point = point % (spline.last() - spline.first());
        }
        return point;
    }
}
