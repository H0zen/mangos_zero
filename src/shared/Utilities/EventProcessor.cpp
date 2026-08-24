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

#include <utility>
#include "EventProcessor.h"
#include "Utilities/AllocMetrics.h"

EventProcessor::~EventProcessor()
{
    KillAllEvents(true);
}

bool EventProcessor::DeliverAbort(BasicEvent& event, uint64 time)
{
    if (event.m_aborted)
    {
        return false;
    }

    event.m_aborted = true;
    event.Abort(time);
    return true;
}

/**
 * @brief Advance the clock and run everything now due.
 */
void EventProcessor::Update(uint32 p_time)
{
    m_time += p_time;
    ++m_pass;

    while (!m_events.empty())
    {
        // Skip past anything re-queued during THIS pass. An event that re-queues
        // itself for a time that has already arrived would otherwise run again
        // inside the same Update, forever, with the clock standing still.
        //
        // Skipping rather than breaking out is the point: the queue is ordered by
        // time, so a single event re-queued to `now` sits at the front, and a
        // `break` there would abandon every other event that is also due --
        // including ones queued long ago. Starvation, silent, and dependent on
        // which event happened to sort first.
        EventList::iterator it = m_events.begin();
        while (it != m_events.end() && it->first <= m_time &&
               it->second && it->second->m_queuedPass == m_pass)
        {
            ++it;
        }

        if (it == m_events.end() || it->first > m_time)
        {
            break;
        }

        // Ownership moves OUT of the map before the event runs. That ordering is
        // what makes re-entrancy safe: while Execute is running, the map holds
        // no pointer to this event, so anything Execute triggers -- another
        // Update, a KillAllEvents, the owner's destruction -- cannot reach it
        // and cannot destroy it twice.
        std::unique_ptr<BasicEvent> event = std::move(it->second);
        m_events.erase(it);

        if (!event)
        {
            continue;
        }

        if (event->IsAbortRequested())
        {
            DeliverAbort(*event, m_time);
            continue;                                   // the unique_ptr destroys it
        }

        if (!event->Execute(m_time, p_time))
        {
            // The event re-queued itself, here or elsewhere, and that insertion
            // is now its owner. Releasing is what stops this unique_ptr from
            // destroying an object the queue is holding.
            event.release();
        }
    }
}

/**
 * @brief Cancel everything.
 *
 * @param force true destroys every event regardless of IsDeletable().
 */
void EventProcessor::KillAllEvents(bool force)
{
    // Read by AddEvent and Reschedule, which refuse while it is set.
    //
    // ===== SAVED AND RESTORED, BECAUSE THIS NESTS =====
    //
    // Abort() is virtual and runs game code, and game code can call
    // KillAllEvents again. An inner call that finished by storing `false`
    // outright would leave the OUTER call delivering aborts with the guard
    // switched off. Restoring the previous value makes the depth irrelevant:
    // the inner call puts back `true`, only the outermost puts back `false`.
    // ==================================================
    bool const wasAborting = m_aborting;
    m_aborting = true;

    // ===== NOT ITERATED WHILE GAME CODE RUNS =====
    //
    // Abort is virtual, it runs game code, and that code can call KillAllEvents
    // again -- which would swap the map out from under an iterator this loop was
    // holding. So the queue is moved out first and the survivors put back after.
    // While the handlers run, m_events is a container they may do anything to.
    // =============================================
    EventList pending;
    pending.swap(m_events);

    EventList survivors;

    for (EventList::iterator itr = pending.begin(); itr != pending.end(); ++itr)
    {
        if (!itr->second)
        {
            continue;
        }

        itr->second->RequestAbort();
        DeliverAbort(*itr->second, m_time);

        if (!force && !itr->second->IsDeletable())
        {
            // Stays queued. A later Update finds the abort request and destroys
            // it -- WITHOUT calling Abort again, because m_aborted is now set.
            survivors.insert(EventList::value_type(itr->first, std::move(itr->second)));
        }
    }

    // ===== DESTROYED BEFORE THE GUARD COMES BACK DOWN =====
    //
    // Everything still held by `pending` dies here, and ~BasicEvent is virtual:
    // a SpellEvent's destructor cancels its spell, which runs game code, which
    // can cast, which queues an event. If that happens after m_aborting has been
    // restored to false, the insertion is ACCEPTED -- into a processor whose
    // owner is being destroyed. That is precisely the case the guard exists for,
    // reached through the destructors instead of through Abort.
    //
    // So the queue is emptied while the guard is still up, and only then is the
    // flag put back.
    // ======================================================
    pending.clear();

    // Whatever a handler queued in the meantime keeps its place; the survivors
    // are merged back in rather than overwriting it.
    for (EventList::iterator itr = survivors.begin(); itr != survivors.end(); ++itr)
    {
        m_events.insert(EventList::value_type(itr->first, std::move(itr->second)));
    }

    m_aborting = wasAborting;
}

/**
 * @brief Queue an event, taking ownership.
 *
 * @return false if the processor is tearing down; the event has been aborted and
 *         destroyed rather than queued.
 */
bool EventProcessor::AddEvent(std::unique_ptr<BasicEvent> event, uint64 e_time, bool set_addtime)
{
    if (!event)
    {
        return false;
    }

    if (m_aborting)
    {
        // Refused, and cleaned up rather than dropped: the event is aborted so
        // that whatever it holds is released, then destroyed when `event` goes
        // out of scope one line below. The caller's pointer to it is dangling
        // from here on, which is why the return value is not decorative.
        event->RequestAbort();
        DeliverAbort(*event, m_time);
        return false;
    }

    if (set_addtime)
    {
        event->m_addTime = m_time;
    }
    event->m_execTime = e_time;
    event->m_queuedPass = m_pass;

    AllocMetrics::Count(AllocMetrics::SITE_EVENT_QUEUE);
    m_events.insert(EventList::value_type(e_time, std::move(event)));
    return true;
}

/**
 * @brief Re-queue an event that is executing right now.
 *
 * @return false if the processor is tearing down; the event is NOT adopted.
 */
bool EventProcessor::Reschedule(BasicEvent* event, uint64 e_time, bool set_addtime)
{
    if (!event)
    {
        return false;
    }

    if (m_aborting)
    {
        return false;
    }

    // Guards the same-processor half of the double-ownership hazard: adopting an
    // event this map already holds would put two unique_ptrs on one object. It
    // cannot see the cross-processor half -- an event owned by SOMEBODY ELSE's
    // queue -- which is why the contract is "only from inside your own Execute",
    // where Update has already released ownership.
    for (EventList::const_iterator itr = m_events.begin(); itr != m_events.end(); ++itr)
    {
        if (itr->second.get() == event)
        {
            return false;
        }
    }

    if (set_addtime)
    {
        event->m_addTime = m_time;
    }
    event->m_execTime = e_time;
    event->m_queuedPass = m_pass;

    AllocMetrics::Count(AllocMetrics::SITE_EVENT_QUEUE);
    m_events.insert(EventList::value_type(e_time, std::unique_ptr<BasicEvent>(event)));
    return true;
}
