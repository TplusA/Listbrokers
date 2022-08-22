/*
 * Copyright (C) 2017, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#include "cacheable.hh"

constexpr const std::chrono::seconds Cacheable::Override::EXPIRY_TIME;

static int trampoline(void *user_data)
{
    auto ovr = static_cast<Cacheable::Override *>(user_data);
    log_assert(ovr != nullptr);

    if(ovr->is_invalidated() || ovr->is_timeout_exceeded())
        ovr->expired_fn_();

    return false;
}

std::chrono::seconds Cacheable::Override::keep_alive()
{
    do_invalidate(false);

    glib_wrapper_.create_timeout(start_time_, active_timer_id_, trampoline, this);
    log_assert(active_timer_id_ != 0);

    return EXPIRY_TIME;
}

bool Cacheable::Override::is_on_path_to_override(ID::List list_id) const
{
    return nodes_on_overridden_path_to_root_.find(list_id) != nodes_on_overridden_path_to_root_.end();
}

void Cacheable::Override::do_invalidate(bool may_call_expiry_callback)
{
    if(active_timer_id_ != 0)
        glib_wrapper_.remove_timeout(active_timer_id_);

    active_timer_id_ = 0;

    if(!is_invalidated())
    {
        start_time_ = INT64_MIN;

        if(may_call_expiry_callback)
            expired_fn_();
    }
}

bool Cacheable::Override::is_timeout_exceeded() const
{
    log_assert(start_time_ > INT64_MIN);
    return glib_wrapper_.has_t_exceeded_expiry_time(start_time_);
}

void Cacheable::Override::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());
    log_assert(replacement_id.is_valid());

    auto it = nodes_on_overridden_path_to_root_.find(list_id);

    if(it != nodes_on_overridden_path_to_root_.end())
    {
        nodes_on_overridden_path_to_root_.emplace(replacement_id, std::move(it->second));
        nodes_on_overridden_path_to_root_.erase(it);
    }
}

std::chrono::seconds Cacheable::CheckWithOverrides::put_override(ID::List list_id)
{
    log_assert(list_id.is_valid());

    auto e = cache_.lookup(list_id);

    if(e == nullptr)
        return std::chrono::seconds(-1);

    std::map<const ID::List, const bool> nodes;

    for(e = e->get_parent(); e != nullptr; e = e->get_parent())
    {
        if(e->get_cache_id().get_nocache_bit())
            nodes.emplace(e->get_cache_id(), true);
        else
            break;
    }

    auto it(overrides_.emplace(list_id,
                               Override(glib_wrapper_, std::move(nodes),
                                        [this, list_id] () { expired(list_id); })));

    return it.first->second.keep_alive();
}

bool Cacheable::CheckWithOverrides::remove_override(ID::List list_id)
{
    auto it(overrides_.find(list_id));

    if(it == overrides_.end())
        return false;

    /* we must keep the Override object around because the timer expiry
     * callback may still be called for it */
    it->second.invalidate();

    return true;
}

void Cacheable::CheckWithOverrides::expired(ID::List list_id)
{
    auto it(overrides_.find(list_id));

    if(it == overrides_.end())
        return;

    overrides_.erase(list_id);
}

bool Cacheable::CheckWithOverrides::is_cacheable(ID::List list_id) const
{
    if(!list_id.is_valid())
        return false;

    auto e = cache_.lookup(list_id);

    if(e == nullptr)
    {
        BUG("No list in cache for ID %u", list_id.get_raw_id());
        return false;
    }

    if(!list_id.get_nocache_bit())
        return true;

    if(overrides_.empty())
        return false;

    for(const auto &ovr : overrides_)
    {
        if(ovr.first == list_id)
            return true;

        if(ovr.second.is_on_path_to_override(list_id))
            return true;
    }

    for(e = e->get_parent(); e != nullptr; e = e->get_parent())
    {
        const auto it(overrides_.find(e->get_cache_id()));

        if(it != overrides_.end() && !it->second.is_invalidated())
            return true;
    }

    return false;
}

static void invalidate_override(std::map<ID::List, Cacheable::Override> &overrides,
                                ID::List list_id, ID::List replacement_id)
{
    auto it = overrides.find(list_id);

    if(it != overrides.end() && list_id != replacement_id)
    {
        overrides.emplace(replacement_id, std::move(it->second));
        overrides.erase(it);
    }
}

static void patch_paths_to_root(std::map<ID::List, Cacheable::Override> &overrides,
                                ID::List list_id, ID::List replacement_id)
{
    for(auto &it : overrides)
        it.second.list_invalidate(list_id, replacement_id);
}

void Cacheable::CheckWithOverrides::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    if(!list_id.is_valid())
        return;

    if(overrides_.empty())
        return;

    if(replacement_id.is_valid())
    {
        invalidate_override(overrides_, list_id, replacement_id);
        patch_paths_to_root(overrides_, list_id, replacement_id);
    }
    else
    {
        remove_override(list_id);

        /* deletion of any overrides for lists deeper down the tree are going
         * to be handled by the case above as soon as those lists are
         * invalidated as well, so let's wait for it */
    }
}
