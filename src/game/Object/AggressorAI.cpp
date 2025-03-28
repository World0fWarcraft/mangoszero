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

#include "AggressorAI.h"
#include "Errors.h"
#include "Creature.h"
#include "World.h"
#include "DBCStores.h"
#include "Map.h"
#include "Log.h"

int AggressorAI::Permissible(const Creature* creature)
{
    // have some hostile factions, it will be selected by IsHostileTo check at MoveInLineOfSight
    if (!(creature->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_AGGRO) && !creature->IsNeutralToAll())
    {
        return PERMIT_BASE_PROACTIVE;
    }

    return PERMIT_BASE_NO;
}

AggressorAI::AggressorAI(Creature* c) : CreatureAI(c), i_state(STATE_NORMAL), i_tracker(TIME_INTERVAL_LOOK)
{
}

void AggressorAI::MoveInLineOfSight(Unit* u)
{
    // Ignore Z for flying creatures
    if (!m_creature->CanFly() && m_creature->GetDistanceZ(u) > CREATURE_Z_ATTACK_RANGE)
    {
        return;
    }

    if (m_creature->CanInitiateAttack() && u->IsTargetableForAttack() &&
        m_creature->IsHostileTo(u) && u->isInAccessablePlaceFor(m_creature))
    {
        float attackRadius = m_creature->GetAttackDistance(u);
        if (m_creature->IsWithinDistInMap(u, attackRadius) && m_creature->IsWithinLOSInMap(u))
        {
            if (!m_creature->getVictim())
            {
                AttackStart(u);
            }
            else if (sMapStore.LookupEntry(m_creature->GetMapId())->IsDungeon())
            {
                m_creature->AddThreat(u);
                u->SetInCombatWith(m_creature);
            }
        }
    }
}

void AggressorAI::EnterEvadeMode()
{
    if (!m_creature->IsAlive())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, he is dead [guid=%u]", m_creature->GetGUIDLow());
        i_victimGuid.Clear();
        m_creature->CombatStop(true);
        m_creature->DeleteThreatList();
        return;
    }

    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    if (!victim)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, no victim [guid=%u]", m_creature->GetGUIDLow());
    }
    else if (!victim->IsAlive())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, victim is dead [guid=%u]", m_creature->GetGUIDLow());
    }
    else if (victim->HasStealthAura())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, victim is in stealth [guid=%u]", m_creature->GetGUIDLow());
    }
    else if (victim->IsTaxiFlying())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, victim is in flight [guid=%u]", m_creature->GetGUIDLow());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking, victim out run him [guid=%u]", m_creature->GetGUIDLow());
        // i_state = STATE_LOOK_AT_VICTIM;
        // i_tracker.Reset(TIME_INTERVAL_LOOK);
    }

    if (!m_creature->IsCharmed())
    {
        m_creature->RemoveAllAurasOnEvade();

        // Remove ChaseMovementGenerator from MotionMaster stack list, and add HomeMovementGenerator instead
        if (m_creature->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
            m_creature->GetMotionMaster()->MoveTargetedHome();
        }
    }

    m_creature->DeleteThreatList();
    i_victimGuid.Clear();
    m_creature->CombatStop(true);
    m_creature->SetLootRecipient(NULL);

    // Reset back to default spells template. This also resets timers.
    SetSpellsList(m_creature->GetCreatureInfo()->SpellListId);
}

void AggressorAI::UpdateAI(const uint32 diff)
{
    // update i_victimGuid if m_creature->getVictim() !=0 and changed
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
    {
        return;
    }

    i_victimGuid = m_creature->getVictim()->GetObjectGuid();

    if (!m_CreatureSpells.empty())
    {
        UpdateSpellsList(diff);
    }

    DoMeleeAttackIfReady();
}

bool AggressorAI::IsVisible(Unit* pl) const
{
    return m_creature->IsWithinDist(pl, sWorld.getConfig(CONFIG_FLOAT_SIGHT_MONSTER))
           && pl->IsVisibleForOrDetect(m_creature, m_creature, true);
}

void AggressorAI::AttackStart(Unit* u)
{
    if (!u)
    {
        return;
    }

    if (m_creature->Attack(u, true))
    {
        i_victimGuid = u->GetObjectGuid();

        m_creature->AddThreat(u);
        m_creature->SetInCombatWith(u);
        u->SetInCombatWith(m_creature);

        HandleMovementOnAttackStart(u);
    }
}
