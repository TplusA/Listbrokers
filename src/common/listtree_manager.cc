/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#include "listtree_manager.hh"
#include "dbus_lists_iface.hh"

void ListTreeManager::announce_root_list(ID::List id)
{
    log_assert(id.is_valid());

    cache_check_.list_invalidate(ID::List(), id);

    auto *iface = DBusNavlists::get_navigation_iface();
    tdbus_lists_navigation_emit_list_invalidate(iface, 0, id.get_raw_id());
}

void ListTreeManager::reinsert_list(ID::List &id)
{
    std::shared_ptr<LRU::Entry> list(cache_.lookup(id));
    ID::List old_id = list->get_cache_id();

    id = cache_.insert_again(std::move(list));
    log_assert(id != old_id);

    cache_check_.list_invalidate(old_id, id);

    auto *iface = DBusNavlists::get_navigation_iface();
    tdbus_lists_navigation_emit_list_invalidate(iface, old_id.get_raw_id(),
                                                id.get_raw_id());
}

bool ListTreeManager::use_list(ID::List id, bool pin_it)
{
    if(!id.is_valid())
        return false;

    if(cache_.use(id) == LRU::Cache::USED_ENTRY_INVALID_ID)
        return false;

    if(!pin_it)
        return true;

    ID::List previous_pinned = cache_.get_pinned_object();

    if(!cache_.pin(id) && previous_pinned.is_valid())
        cache_.pin(previous_pinned);

    return true;
}

std::chrono::seconds
ListTreeManager::force_list_into_cache(ID::List list_id, bool force)
{
    if(force)
    {
        return std::max(cache_check_.put_override(list_id),
                        std::chrono::seconds::zero());
    }
    else
    {
        cache_check_.remove_override(list_id);
        return std::chrono::seconds::zero();
    }
}

void ListTreeManager::repin_if_first_is_deepest_pinned_list(ID::List first_id,
                                                            ID::List other_id)
{
    log_assert(other_id.is_valid());

    if(!first_id.is_valid())
        return;

    if(first_id == cache_.get_pinned_object())
        cache_.pin(other_id);
}

void ListTreeManager::list_discarded_from_cache(ID::List id)
{
    cache_check_.list_invalidate(id, ID::List());

    auto *iface = DBusNavlists::get_navigation_iface();
    tdbus_lists_navigation_emit_list_invalidate(iface, id.get_raw_id(), 0);
}

ListTreeManager::PurgeResult
ListTreeManager::purge_subtree(ID::List old_id, ID::List new_id,
                               const EnterChild::SetNewRoot &set_root)
{
    auto list = old_id.is_valid() ? cache_.lookup(old_id) : nullptr;

    if(list == nullptr)
    {
        if(set_root != nullptr)
            set_root(old_id, new_id);

        return PurgeResult::INVALID;
    }

    std::vector<ID::List> kill_list;
    list->enumerate_tree_of_sublists(cache_, kill_list);

    log_assert(kill_list.begin() != kill_list.end());

    size_t first_to_kill;
    ListTreeManager::PurgeResult result;

    if(!new_id.is_valid())
    {
        first_to_kill = 0;
        result = PurgeResult::PURGED;
    }
    else if(old_id == new_id)
    {
        first_to_kill = 1;
        result = kill_list.size() > 1
            ? PurgeResult::PURGED
            : PurgeResult::UNTOUCHED;
    }
    else
    {
        first_to_kill = 1;
        result = kill_list.size() > 1
            ? PurgeResult::PURGED_AND_REPLACED
            : PurgeResult::REPLACED_ROOT;
    }

    bool need_to_process_kill_list = false;

    switch(result)
    {
      case PurgeResult::INVALID:
      case PurgeResult::UNTOUCHED:
      case PurgeResult::PURGED:
        if(set_root != nullptr)
            set_root(old_id, new_id);

        need_to_process_kill_list = (result == PurgeResult::PURGED);

        break;

      case PurgeResult::REPLACED_ROOT:
      case PurgeResult::PURGED_AND_REPLACED:
        cache_.purge_entries(kill_list.begin(), kill_list.begin() + 1, false);

        if(set_root != nullptr)
            set_root(old_id, new_id);

        cache_check_.list_invalidate(old_id, new_id);

        tdbus_lists_navigation_emit_list_invalidate(DBusNavlists::get_navigation_iface(),
                                                    old_id.get_raw_id(),
                                                    new_id.get_raw_id());

        need_to_process_kill_list = (result == PurgeResult::PURGED_AND_REPLACED);

        break;
    }

    if(need_to_process_kill_list)
    {
        cache_.toposort_for_purge(kill_list.begin() + first_to_kill, kill_list.end());
        cache_.purge_entries(kill_list.begin() + first_to_kill, kill_list.end());
    }

    return result;
}
