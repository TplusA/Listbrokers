/*
 * Copyright (C) 2015, 2016, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#include "cachecontrol.hh"

LRU::CacheControl::~CacheControl()
{
    disable_garbage_collection();

    if(loop_ != NULL)
        g_main_loop_unref(loop_);
}

static gboolean trampoline(gpointer user_data)
{
    auto ctrl = static_cast<LRU::CacheControl *>(user_data);
    msg_log_assert(ctrl != nullptr);

    ctrl->trigger_gc();

    return G_SOURCE_REMOVE;
}

void LRU::CacheControl::trigger_gc()
{
    msg_log_assert(loop_ != NULL);

    msg_info("Garbage collection triggered");
    gc_and_set_timeout();
    msg_info("Garbage collection done");
}

void LRU::CacheControl::gc_and_set_timeout()
{
    timeout_source_id_ = 0;
    timeout_source_ = NULL;

    if(is_enabled_)
        set_timeout(cache_.gc());
    else
        msg_info("Garbage collection disabled");
}

void LRU::CacheControl::set_timeout(std::chrono::seconds timeout)
{
    if(timeout == std::chrono::seconds::max())
        return;

    /* just in case we dropped in a bit too soon with our garbage collection as
     * result of rounding errors, clock skews, scheduling oddities, or
     * whatever; also prevents CPU hogging in case the timeout successively
     * evaluates as 0 */
    static constexpr guint minimum_timeout_ms = 500;

    const auto timeout_ms = std::chrono::milliseconds(timeout);
    const guint source_timeout_ms =
        (timeout_ms.count() <= G_MAXUINT
         ? ((timeout_ms.count() >= minimum_timeout_ms)
            ? timeout_ms.count()
            : minimum_timeout_ms)
         : G_MAXUINT);

    msg_info("Garbage collection timeout %u ms", source_timeout_ms);

    timeout_source_ = g_timeout_source_new(source_timeout_ms);
    msg_log_assert(timeout_source_ != NULL);

    g_source_set_callback(timeout_source_, trampoline, this, NULL);
    timeout_source_id_ = g_source_attach(timeout_source_, NULL);
}

void LRU::CacheControl::enable_garbage_collection()
{
    msg_log_assert(loop_ != NULL);

    is_enabled_ = true;

    if(timeout_source_id_ == 0)
        gc_and_set_timeout();
}

void LRU::CacheControl::disable_garbage_collection()
{
    /* keep timeout enabled, let the timeout handler deal with it */
    is_enabled_ = false;
}
