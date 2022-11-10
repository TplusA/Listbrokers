/*
 * Copyright (C) 2022  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A List Brokers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "work_by_cookie.hh"

uint32_t DBusAsync::CookieJar::pick_cookie_for_work(
        std::shared_ptr<CookiedWorkBase> &&work,
        DataAvailableNotificationMode mode)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> jar_lock(lock_);

    const auto cookie = bake_cookie();
    work->set_done_notification_function(
        [this, cookie, mode]
        (LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock, bool has_completed)
        { work_done_notification(work_lock, cookie, mode, has_completed); });
    work_by_cookie_.emplace(cookie, std::move(work));

    return cookie;
}

void DBusAsync::CookieJar::cookie_not_wanted(uint32_t cookie)
{
    std::shared_ptr<CookiedWorkBase> w;

    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> jar_lock(lock_);

        auto it(work_by_cookie_.find(cookie));
        if(it == work_by_cookie_.end())
            return;

        /* The cancel() function called below is likely to end up in
         * #CookieJar::work_done_notification(), which will (1) lock this
         * object, and (2) erase the work from the container. To avoid a
         * deadlock and to avoid UB, we reference the work item and cancel
         * it with this object unlocked. */
        w = it->second;
    }

    msg_log_assert(w != nullptr);
    w->cancel();
}

void DBusAsync::CookieJar::work_done_notification(
        LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock,
        uint32_t cookie, DataAvailableNotificationMode mode,
        bool has_completed)
{
    LOGGED_LOCK_CONTEXT_HINT;
    LoggedLock::UniqueLock<LoggedLock::Mutex> jar_lock(lock_);

    const auto &work_iter(work_by_cookie_.find(cookie));

    if(work_iter == work_by_cookie_.end())
    {
        /* work has been removed already in #CookieJar::try_eat() */
        return;
    }

    /* the work item is known, so the caller will have it locked for us */
    std::shared_ptr<CookiedWorkBase> work = work_iter->second;
    const bool success = work->success();
    const auto error = has_completed
        ? work->get_error_code()
        : ListError::Code::INTERRUPTED;

    if(!has_completed)
        work_by_cookie_.erase(work_iter);

    /*
     * We need to unlock the cookie jar because
     * #DBusAsync::ReplyPathTracker::try_take_fast_path() may have to wait
     * for a transition of the work's reply state, and this involves
     * running work from the cookie jar.
     *
     * The code below does not make use of the cookie jar object, so it
     * will remain UNLOCKED from this point on. It should be safe to lock
     * the cookie jar again further down the road if required in future
     * versions of this code.
     */
    jar_lock.unlock();

    const DBusAsync::ReplyPathTracker::TakePathResult take_path_result =
        work->reply_path_tracker__unlocked().try_take_fast_path(work_lock);

    switch(take_path_result)
    {
      case DBusAsync::ReplyPathTracker::TakePathResult::TAKEN:
        /* OK, we are done here; the cookie jar needs to collect the result
         * and return it the fast way */
        if(success && mode == DataAvailableNotificationMode::ALWAYS)
        {
            /* ...unless, of course, the work is associated with a D-Bus
             * function which defines a purely asynchronous interface */
            break;
        }

        return;

      case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_NOT_ANNOUNCED_YET:
        MSG_NOT_IMPLEMENTED();
        return;

      case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_ANNOUNCED:
        /* OK, cookie jar has already handled a timeout and we need to
         * announce the availability of the pending result for the cookie
         * now */
        break;

      case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_READY_ANNOUNCED:
        MSG_BUG("Requesting fast path for cookie %u due to completion, but already in slow path phase 2, completed %d", cookie, has_completed);
        return;

      case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_FETCHING:
        MSG_BUG("Requesting fast path for cookie %u due to completion, but already in slow path phase 3, completed %d", cookie, has_completed);
        return;

      case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_FAST_PATH:
        MSG_BUG("Requesting fast path for cookie %u due to completion, but already taking fast path, completed %d", cookie, has_completed);
        return;

      case DBusAsync::ReplyPathTracker::TakePathResult::INVALID:
        MSG_BUG("Requesting fast path for cookie %u due to completion, but this is an invalid transition, completed %d", cookie, has_completed);
        return;
    }

    if(success)
    {
        switch(mode)
        {
          case DataAvailableNotificationMode::NEVER:
            break;

          case DataAvailableNotificationMode::AFTER_TIMEOUT:
          case DataAvailableNotificationMode::ALWAYS:
            work->notify_data_available(cookie);
            work->with_reply_path_tracker__already_locked<bool>(
                work_lock,
                [] (auto &wlock, auto &rpt)
                { return rpt.slow_path_ready_notified_client(wlock); });
            break;
        }
    }
    else
        work->notify_data_error(cookie, error);
}

uint32_t DBusAsync::CookieJar::bake_cookie()
{
    while(true)
    {
        const uint32_t cookie = next_free_cookie_++;

        if(cookie == 0)
            continue;

        if(work_by_cookie_.find(cookie) != work_by_cookie_.end())
            continue;

        return cookie;
    }
}

DBusAsync::CookieJar &DBusAsync::get_cookie_jar_singleton()
{
    static CookieJar cookie_jar;
    return cookie_jar;
}
