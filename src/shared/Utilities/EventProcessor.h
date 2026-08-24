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

#ifndef MANGOS_H_EVENTPROCESSOR
#define MANGOS_H_EVENTPROCESSOR

#include "Platform/Define.h"
#include <map>
#include <memory>

/**
 * @brief Something to happen later. All times are milliseconds.
 *
 * Subclass it, hand it to an EventProcessor, and the processor owns it from that
 * moment until one of two things happens:
 *
 *   Execute returns TRUE   the event is finished and the processor destroys it.
 *
 *   Execute returns FALSE  the event has RE-QUEUED ITSELF, in this processor or
 *                          another one, and this processor gives up ownership
 *                          without destroying it.
 *
 * The second is not hypothetical: a spell in flight re-queues itself on every
 * update until it lands. It is also the sharp edge -- an Execute that returns
 * false without re-queueing leaks the event, silently, and nothing here can
 * detect it, because the event may legitimately have gone to a processor this
 * one has never heard of.
 */
class BasicEvent
{
    public:

        BasicEvent()
            : m_abortRequested(false), m_aborted(false),
              m_addTime(0), m_execTime(0), m_queuedPass(0)
        {
        }

        virtual ~BasicEvent() {}

        // Events are owned through pointers and identified by address; copying
        // one would produce a second object the processor knows nothing about.
        BasicEvent(BasicEvent const&) = delete;
        BasicEvent& operator=(BasicEvent const&) = delete;

        /**
         * @param e_time The processor's clock at the moment of execution.
         * @param p_time Milliseconds since the previous update.
         * @return true to be destroyed, false if the event has re-queued itself.
         */
        virtual bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) { return true; }

        /// False to survive a non-forced KillAllEvents.
        virtual bool IsDeletable() const { return true; }

        /// Called instead of Execute when the event is cancelled. Exactly once.
        virtual void Abort(uint64 /*e_time*/) {}

        /**
         * @brief Ask for this event to be cancelled rather than executed.
         *
         * A request from outside, so it is writable from outside -- but paired
         * with a separate flag the processor owns, so "someone asked" and "the
         * abort has been delivered" stay distinct.
         */
        void RequestAbort() { m_abortRequested = true; }
        bool IsAbortRequested() const { return m_abortRequested; }

        /// When the event was queued, and when it is due.
        uint64 AddedAt() const { return m_addTime; }
        uint64 ScheduledFor() const { return m_execTime; }

    private:

        friend class EventProcessor;

        bool m_abortRequested;

        /**
         * @brief Whether Abort() has already been delivered.
         *
         * A non-forced KillAllEvents aborts every event but leaves the ones that
         * are not yet deletable in the queue; a later Update finds them and
         * destroys them. Without this flag it would abort them a SECOND time --
         * and an Abort that releases a resource, refunds a cost or notifies a
         * player would do it twice, with nothing in the event's own code to
         * suggest that could happen.
         */
        bool m_aborted;

        uint64 m_addTime;
        uint64 m_execTime;

        /**
         * @brief Which Update pass queued this event.
         *
         * An event that re-queues itself for a time that has already arrived
         * would otherwise run again inside the same Update, and again, without
         * the clock ever advancing.
         */
        uint64 m_queuedPass;
};

/**
 * @brief A time-ordered queue of events, driven by the owner's update tick.
 *
 * One of these lives on every Unit. It is not thread-safe and does not need to
 * be: it is driven from the thread that updates its owner.
 */
class EventProcessor
{
    public:

        EventProcessor() : m_time(0), m_pass(0), m_aborting(false) {}
        ~EventProcessor();

        EventProcessor(EventProcessor const&) = delete;
        EventProcessor& operator=(EventProcessor const&) = delete;

        /// Advance the clock and run everything now due.
        void Update(uint32 p_time);

        /**
         * @brief Cancel everything.
         *
         * @param force true destroys every event regardless of IsDeletable().
         *              false leaves undeletable events queued -- they will be
         *              destroyed by a later Update, without a second Abort.
         */
        void KillAllEvents(bool force);

        /**
         * @brief Queue an event, taking ownership.
         *
         * @return false if the processor is tearing down, in which case the
         *         event has been ABORTED AND DESTROYED rather than queued.
         *
         * Read that return value. The event is gone when it is false, so a
         * caller holding a pointer to it -- or to anything the event owns, which
         * for a SpellEvent means the Spell itself -- is holding a dangling one
         * from that instant. `Spell::prepare` is the case that matters and it
         * returns immediately without touching itself again.
         *
         * An Abort handler is exactly where a dying object queues its cleanup,
         * so insertion during teardown is a real path and must be refused --
         * anything accepted then would be dropped by the teardown already in
         * progress.
         */
        bool AddEvent(std::unique_ptr<BasicEvent> event, uint64 e_time, bool set_addtime = true);

        /**
         * @brief Re-queue an event that is executing right now.
         *
         * ONLY legal from inside that event's own Execute, which must then
         * return false. That pair is the contract: Execute says "I did not
         * finish", and this says where the still-living event went.
         *
         * Takes a raw pointer BECAUSE the event is mid-Execute and cannot hand
         * over a unique_ptr to itself. Update released ownership before calling
         * Execute, so exactly one owner exists at every moment.
         *
         * @return false if the target processor is tearing down; the event is
         *         then NOT adopted and the caller still owns it -- and since the
         *         caller is the event, returning false from Execute after this
         *         leaks it. Callers must destroy themselves instead.
         */
        bool Reschedule(BasicEvent* event, uint64 e_time, bool set_addtime = false);

        /// The processor's clock plus an offset -- how callers name a due time.
        uint64 CalculateTime(uint64 t_offset) const { return m_time + t_offset; }

        uint64 Now() const { return m_time; }

        bool IsEmpty() const { return m_events.empty(); }

    protected:

        /// Deliver Abort exactly once. Returns false if it had already been sent.
        static bool DeliverAbort(BasicEvent& event, uint64 time);

        typedef std::multimap<uint64, std::unique_ptr<BasicEvent> > EventList;

        EventList m_events;

        uint64 m_time;
        uint64 m_pass;
        bool m_aborting;
};

#endif
