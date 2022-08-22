/*
 * Copyright (C) 2017, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef CACHEABLE_GLIB_HH
#define CACHEABLE_GLIB_HH

#include "cacheable.hh"

#include <glib.h>

namespace Cacheable
{

class GLibWrapper: public GLibWrapperIface
{
  public:
    GLibWrapper(const GLibWrapper &) = delete;
    GLibWrapper(GLibWrapper &&) = default;
    GLibWrapper &operator=(const GLibWrapper &) = delete;

    explicit GLibWrapper() {}

    void ref_main_loop(struct _GMainLoop *loop) const final override
    {
        g_main_loop_ref(loop);
    }

    void unref_main_loop(struct _GMainLoop *loop) const final override
    {
        g_main_loop_unref(loop);
    }

    void create_timeout(int64_t &start_time, uint32_t &active_timer_id,
                        int (*trampoline)(void *user_data),
                        Override *origin_object) const final override
    {
        start_time = g_get_monotonic_time();
        active_timer_id = g_timeout_add_seconds(Override::EXPIRY_TIME.count(),
                                                trampoline, origin_object);
    }

    void remove_timeout(uint32_t active_timer_id) const final override
    {
        auto *src = g_main_context_find_source_by_id(nullptr, active_timer_id);

        if(src != nullptr)
            g_source_destroy(src);
    }

    bool has_t_exceeded_expiry_time(int64_t t) const final override
    {
        return (g_get_monotonic_time() - t) >= std::chrono::microseconds(Override::EXPIRY_TIME).count();
    }
};

}

#endif /* !CACHEABLE_GLIB_HH */
