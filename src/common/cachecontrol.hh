/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A List Brokers.
 *
 * T+A List Brokers is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * T+A List Brokers is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with T+A List Brokers.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CACHECONTROL_HH
#define CACHECONTROL_HH

#include <glib.h>

#include "lru.hh"

namespace LRU
{

/*!
 * Managing periodic garbage collection by attaching to a GLib main loop.
 *
 * This class registers a timeout signal with the given GLib main loop and uses
 * the return value of #LRU::Cache::gc() to program that timeout. When the
 * timeout expires, the garbage collector is started.
 */
class CacheControl
{
  private:
    LRU::Cache &cache_;
    GMainLoop *const loop_;
    GSource *timeout_source_;
    guint timeout_source_id_;
    bool is_enabled_;

  public:
    CacheControl(const CacheControl &) = delete;
    CacheControl &operator=(const CacheControl &) = delete;

    explicit CacheControl(Cache &cache, GMainLoop *loop):
        cache_(cache),
        loop_(loop),
        timeout_source_(NULL),
        timeout_source_id_(0),
        is_enabled_(false)
    {
        g_main_loop_ref(loop);
    }

    ~CacheControl();

    void enable_garbage_collection();
    void disable_garbage_collection();

    void trigger_gc();

  private:
    void set_timeout(std::chrono::seconds timeout);
    void gc_and_set_timeout();
};

};

#endif /* !CACHECONTROL_HH */
