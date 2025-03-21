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

#include "GuardAI.h"
#include "Creature.h"
#include "Player.h"
#include "World.h"

int GuardAI::Permissible(const Creature* creature)
{
    if (creature->IsGuard())
    {
        return PERMIT_BASE_SPECIAL;
    }

    return PERMIT_BASE_NO;
}

GuardAI::GuardAI(Creature* c) : CreatureAI(c), i_state(STATE_NORMAL), i_tracker(TIME_INTERVAL_LOOK)
{
}

void GuardAI::MoveInLineOfSight(Unit* u)
{
    // Ignore Z for flying creatures
    if (!m_creature->CanFly() && m_creature->GetDistanceZ(u) > CREATURE_Z_ATTACK_RANGE)
    {
        return;
    }

    if (!m_creature->getVictim() && u->IsTargetableForAttack() &&
        (u->IsHostileToPlayers() || m_creature->IsHostileTo(u) /*|| u->getVictim() && m_creature->IsFriendlyTo(u->getVictim())*/) &&
        u->isInAccessablePlaceFor(m_creature))
    {
        float attackRadius = m_creature->GetAttackDistance(u);
        if (m_creature->IsWithinDistInMap(u, attackRadius))
        {
            // Need add code to let guard support player
            AttackStart(u);
        }
    }
}

void GuardAI::EnterEvadeMode()
{
    if (!m_creature->IsAlive())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature stopped attacking because he's dead [guid=%u]", m_creature->GetGUIDLow());
        m_creature->StopMoving();
        m_creature->GetMotionMaster()->MoveIdle();

        i_state = STATE_NORMAL;

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
    }

    m_creature->RemoveAllAurasOnEvade();
    m_creature->DeleteThreatList();
    i_victimGuid.Clear();
    m_creature->CombatStop(true);
    i_state = STATE_NORMAL;

    // Remove ChaseMovementGenerator from MotionMaster stack list, and add HomeMovementGenerator instead
    if (m_creature->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
    {
        m_creature->GetMotionMaster()->MoveTargetedHome();
    }

    // Reset back to default spells template. This also resets timers.
    SetSpellsList(m_creature->GetCreatureInfo()->SpellListId);
}

void GuardAI::UpdateAI(const uint32 diff)
{
    // update i_victimGuid if i_creature.getVictim() !=0 and changed
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

bool GuardAI::IsVisible(Unit* pl) const
{
    return m_creature->IsWithinDist(pl, sWorld.getConfig(CONFIG_FLOAT_SIGHT_GUARDER))
           && pl->IsVisibleForOrDetect(m_creature, m_creature, true);
}

void GuardAI::AttackStart(Unit* u)
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

void GuardAI::JustDied(Unit* killer)
{
    if (Player* pkiller = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        m_creature->SendZoneUnderAttackMessage(pkiller);
    }
}
