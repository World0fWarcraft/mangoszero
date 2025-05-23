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

#include "MoveSplineFlag.h"
#include <math.h>

namespace Movement
{
    // Gravity constant used in movement calculations
    double gravity = 19.29110527038574;

    /// Velocity bounds that makes fall speed limited
    float terminalVelocity = 60.148003f;
    float terminalSavefallVelocity = 7.f;

    // Precomputed constants for terminal velocity and fall time
    const float terminal_length = float(terminalVelocity * terminalVelocity) / (2.f * gravity);
    const float terminal_savefall_length = (terminalSavefallVelocity * terminalSavefallVelocity) / (2.f * gravity);
    const float terminalFallTime = float(terminalVelocity / gravity); // the time needed to reach terminalVelocity

    /**
     * @brief Computes the fall time based on the path length and whether it is a safe fall.
     * @param path_length The length of the fall path.
     * @param isSafeFall True if it is a safe fall, false otherwise.
     * @return float The computed fall time.
     */
    float computeFallTime(float path_length, bool isSafeFall)
    {
        if (path_length < 0.f)
        {
            return 0.f;
        }

        float time;
        if (isSafeFall)
        {
            if (path_length >= terminal_savefall_length)
            {
                time = (path_length - terminal_savefall_length) / terminalSavefallVelocity + terminalSavefallVelocity / gravity;
            }
            else
            {
                time = sqrtf(2.f * path_length / gravity);
            }
        }
        else
        {
            if (path_length >= terminal_length)
            {
                time = (path_length - terminal_length) / terminalVelocity + terminalFallTime;
            }
            else
            {
                time = sqrtf(2.f * path_length / gravity);
            }
        }

        return time;
    }

    /**
     * @brief Computes the fall elevation based on the time passed, whether it is a safe fall, and the start velocity.
     * @param t_passed The time passed.
     * @param isSafeFall True if it is a safe fall, false otherwise.
     * @param start_velocity The start velocity.
     * @return float The computed fall elevation.
     */
    float computeFallElevation(float t_passed, bool isSafeFall, float start_velocity)
    {
        float termVel;
        float result;

        if (isSafeFall)
        {
            termVel = terminalSavefallVelocity;
        }
        else
        {
            termVel = terminalVelocity;
        }

        if (start_velocity > termVel)
        {
            start_velocity = termVel;
        }

        float terminal_time = terminalFallTime - start_velocity / gravity; // the time needed to reach terminalVelocity

        if (t_passed > terminal_time)
        {
            result = terminalVelocity * (t_passed - terminal_time) +
                     start_velocity * terminal_time + gravity * terminal_time * terminal_time * 0.5f;
        }
        else
        {
            result = t_passed * (start_velocity + t_passed * gravity * 0.5f);
        }

        return result;
    }

    /**
     * @brief Computes the fall elevation based on the time passed.
     * @param t_passed The time passed.
     * @return float The computed fall elevation.
     */
    float computeFallElevation(float t_passed)
    {
        float result;

        if (t_passed > terminalFallTime)
        {
            // result = terminalVelocity * (t_passed - terminal_time) + gravity*terminal_time*terminal_time*0.5f;
            // simplified view:
            result = terminalVelocity * (t_passed - terminalFallTime) + terminal_length;
        }
        else
        {
            result = t_passed * t_passed * gravity * 0.5f;
        }

        return result;
    }

#define STR(x) #x

    // Array of movement flag names
    const char* g_MovementFlag_names[] =
    {
        STR(Forward),            // 0x00000001,
        STR(Backward),           // 0x00000002,
        STR(Strafe_Left),        // 0x00000004,
        STR(Strafe_Right),       // 0x00000008,
        STR(Turn_Left),          // 0x00000010,
        STR(Turn_Right),         // 0x00000020,
        STR(Pitch_Up),           // 0x00000040,
        STR(Pitch_Down),         // 0x00000080,

        STR(Walk),               // 0x00000100,             // Walking
        STR(Ontransport),        // 0x00000200,
        STR(Levitation),         // 0x00000400,
        STR(Root),               // 0x00000800,
        STR(Falling),            // 0x00001000,
        STR(Fallingfar),         // 0x00002000,
        STR(Pendingstop),        // 0x00004000,
        STR(PendingSTRafestop),  // 0x00008000,
        STR(Pendingforward),     // 0x00010000,
        STR(Pendingbackward),    // 0x00020000,
        STR(PendingSTRafeleft),  // 0x00040000,
        STR(PendingSTRaferight), // 0x00080000,
        STR(Pendingroot),        // 0x00100000,
        STR(Swimming),           // 0x00200000,             // Appears With Fly Flag Also
        STR(Ascending),          // 0x00400000,             // Swim Up Also
        STR(Descending),         // 0x00800000,             // Swim Down Also
        STR(Can_Fly),            // 0x01000000,             // Can Fly In 3.3?
        STR(Flying),             // 0x02000000,             // Actual Flying Mode
        STR(Spline_Elevation),   // 0x04000000,             // Used For Flight Paths
        STR(Spline_Enabled),     // 0x08000000,             // Used For Flight Paths
        STR(Waterwalking),       // 0x10000000,             // Prevent Unit From Falling Through Water
        STR(Safe_Fall),          // 0x20000000,             // Active Rogue Safe Fall Spell (Passive)
        STR(Hover),              // 0x40000000
        STR(Unknown13),          // 0x80000000
        STR(Unk1),
        STR(Unk2),
        STR(Unk3),
        STR(Fullspeedturning),
        STR(Fullspeedpitching),
        STR(Allow_Pitching),
        STR(Unk4),
        STR(Unk5),
        STR(Unk6),
        STR(Unk7),
        STR(Interp_Move),
        STR(Interp_Turning),
        STR(Interp_Pitching),
        STR(Unk8),
        STR(Unk9),
        STR(Unk10),
    };

    // Array of spline flag names
    const char* g_SplineFlag_names[32] =
    {
        STR(Done),             // 0x00000001,
        STR(Falling),          // 0x00000002,
        STR(Unknown3),         // 0x00000004,
        STR(Unknown4),         // 0x00000008,
        STR(Unknown5),         // 0x00000010,
        STR(Unknown6),         // 0x00000020,
        STR(Unknown7),         // 0x00000040,
        STR(Unknown8),         // 0x00000080,
        STR(Runmode),          // 0x00000100,
        STR(Flying),           // 0x00000200,
        STR(No_Spline),        // 0x00000400,
        STR(Unknown12),        // 0x00000800,
        STR(Unknown13),        // 0x00001000,
        STR(Unknown14),        // 0x00002000,
        STR(Unknown15),        // 0x00004000,
        STR(Unknown16),        // 0x00008000,
        STR(Final_Point),      // 0x00010000,
        STR(Final_Target),     // 0x00020000,
        STR(Final_Angle),      // 0x00040000,
        STR(Unknown19),        // 0x00080000,
        STR(Cyclic),           // 0x00100000,
        STR(Enter_Cycle),      // 0x00200000,
        STR(Frozen),           // 0x00400000,
        STR(Unknown23),        // 0x00800000,
        STR(Unknown24),        // 0x01000000,
        STR(Unknown25),        // 0x02000000,
        STR(Unknown26),        // 0x04000000,
        STR(Unknown27),        // 0x08000000,
        STR(Unknown28),        // 0x10000000,
        STR(Unknown29),        // 0x20000000,
        STR(Unknown30),        // 0x40000000,
        STR(Unknown31),        // 0x80000000,
    };

    /**
     * @brief Prints the flags to a string.
     * @tparam Flags The type of the flags.
     * @tparam N The number of flags.
     * @param t The flags.
     * @param names The names of the flags.
     * @param str The string to append the flag names to.
     */
    template<class Flags, int N>
    void print_flags(Flags t, const char * (&names)[N], std::string& str)
    {
        for (int i = 0; i < N; ++i)
        {
            if ((t & (Flags)(1 << i)) && names[i] != NULL)
            {
                str.append(" ").append(names[i]);
            }
        }
    }

    /**
     * @brief Converts the MoveSplineFlag to a string representation.
     * @return std::string The string representation of the MoveSplineFlag.
     */
    std::string MoveSplineFlag::ToString() const
    {
        std::string str;
        print_flags(raw(), g_SplineFlag_names, str);
        return str;
    }
}
