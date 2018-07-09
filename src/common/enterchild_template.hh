/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef ENTERCHILD_TEMPLATE_HH
#define ENTERCHILD_TEMPLATE_HH

#include "enterchild_glue.hh"

namespace EnterChild
{

/*!
 * Generic implementation of entering child list.
 *
 * This function template obtains a reference to the desired child entry in the
 * given list, and then checks whether or not to list is (1) in cache and (2)
 * the cached list may be reused. The function passed in parameter
 * \p use_cached is employed to perform this check.
 *
 * In case the child list must be created (either because there is no cached
 * copy or because returning a cached version is not allowed), the function
 * passed in \p add_to_cache is called. The function is supposed to add the
 * child list into the cache and to return the cache ID of the child list just
 * added.
 *
 * That ID is stored in the \p this_ptr list as reference to the child list.
 * The ID is also stored in case the ID is invalid unless the \p add_to_cache
 * function has set \p error to ListError::INVALID_ID, indicating that the
 * attempt to enter the child was an error in the first place.
 *
 * \param this_ptr
 *     Pointer to list whose child should be entered.
 *
 * \param cache
 *     LRU cache that manages the list passed in \p this_ptr.
 *
 * \param item
 *     Index of the child within given list to enter.
 *
 * \param may_continue
 *     Check whether or not we were interrupted.
 *
 * \param use_cached
 *     Function that should return \c true in case a cached version of the
 *     child is OK to use, \c false in case the child must be reloaded.
 *
 * \param purge_list
 *     Function that unconditionally removes the old child ID from cache. This
 *     is usually a lambda function which simply calls
 *     #ListTreeManager::purge_subtree().
 *
 * \param error
 *     Error codes are returned here.
 *
 * \param add_to_cache
 *     List-specific code for adding the child item to cache. This will usually
 *     make use of the #add_child_list_to_cache() function template and may
 *     perform any extra actions required to add the child to cache. This
 *     function must return the ID of the added child list.
 *
 * \tparam ChildDataType
 *     Type of data stored in the child item.
 *
 * \tparam ContainingListType
 *     Type of \p this_ptr, the list containing the child item to enter.
 *
 * \returns
 *     The ID of a cached child list, if any; or the invalid ID in case the
 *     passed child item is out of range; or the return value of \p
 *     add_to_cache in case a new child list has been created.
 */
template <typename ChildListItemType, typename ContainingListType>
ID::List enter_child_template(ContainingListType *const this_ptr,
                              LRU::Cache &cache, ID::Item item,
                              const std::function<bool()> &may_continue,
                              const EnterChild::CheckUseCached &use_cached,
                              const EnterChild::DoPurgeList &purge_list,
                              ListError &error,
                              const std::function<ID::List(const ChildListItemType &child_entry)> &add_to_cache)
{
    if(!may_continue())
    {
        error = ListError::INTERRUPTED;
        return ID::List();
    }

    error = ListError::OK;

    log_assert(cache.lookup(this_ptr->get_cache_id()) != nullptr);

    if(item.get_raw_id() >= this_ptr->size())
    {
        error = ListError::INVALID_ID;
        return ID::List();
    }

    ChildListItemType &child_entry = (*this_ptr)[item];
    const auto cached_child_id = child_entry.get_child_list();

    if(use_cached(cached_child_id))
    {
        log_assert(cached_child_id.is_valid());
        return cached_child_id;
    }

    return purge_list(cached_child_id, add_to_cache(child_entry),
                      [&child_entry, &error] (ID::List old_id, ID::List new_id)
                      {
                          if(new_id.is_valid() || error != ListError::INVALID_ID)
                              child_entry.set_child_list(new_id);
                      });
}

}

#endif /* !ENTERCHILD_TEMPLATE_HH */
