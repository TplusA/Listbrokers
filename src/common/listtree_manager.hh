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

#ifndef LISTTREE_MANAGER_HH
#define LISTTREE_MANAGER_HH

#include <string>

#include "lru.hh"
#include "i18nstring.hh"
#include "enterchild_glue.hh"
#include "cacheable.hh"
#include "messages.h"
#include "de_tahifi_lists_errors.hh"

/*!
 * Utility class for managing trees of lists.
 *
 * Functions implemented in this class provide higher-level functionalities on
 * top of the raw cache and list data structures. They support the
 * application-level code by adding minimum error detection and providing basic
 * functionalities such as marking a list as used, avoiding code duplication
 * further up the stack.
 */
class ListTreeManager
{
  public:
    enum class PurgeResult
    {
        INVALID,
        UNTOUCHED,
        REPLACED_ROOT,
        PURGED,
        PURGED_AND_REPLACED,
    };

    ListTreeManager(const ListTreeManager &) = delete;
    ListTreeManager &operator=(const ListTreeManager &) = delete;

    explicit ListTreeManager(LRU::Cache &cache, Cacheable::CheckIface &check):
        cache_(cache),
        cache_check_(check),
        default_cache_mode_request_(LRU::CacheModeRequest::AUTO)
    {}

    void set_default_lru_cache_mode(LRU::CacheModeRequest req)
    {
        default_cache_mode_request_ = req;
    }

    /*!
     * Allocate a new, empty list with given parent.
     *
     * This function does not put the list into the cache, it only wraps the
     * allocation of the list.
     *
     * In addition, the list is also remembered internally to trace its use,
     * marking it pending. At any time, there can be no more than a single
     * pending list. When blessing a list (putting it into cache and assigning
     * a cache object ID), it is expected to be the pending list. This
     * mechanism is supposed to find certain bugs such as leaking lists.
     */
    template <typename T, typename... Args>
    std::shared_ptr<T> allocate_list(const std::shared_ptr<LRU::Entry> &parent,
                                     Args&&... args)
    {
        log_assert(pending_list_ == nullptr);

        auto l = std::make_shared<T>(parent, args...);
        log_assert(l != nullptr);

        pending_list_ = l;

        return l;
    }

    template <typename T, typename... Args>
    ID::List allocate_blessed_list(const std::shared_ptr<LRU::Entry> &parent,
                                   // cppcheck-suppress passedByValue
                                   const ID::List::context_t ctx,
                                   size_t size_of_list, bool pin_it,
                                   Args&&... args)
    {
        auto l = allocate_list<T>(parent, args...);

        return bless(l, ctx, size_of_list, pin_it);
    }

    ID::List bless(std::shared_ptr<LRU::Entry> &&list,
                   // cppcheck-suppress passedByValue
                   const ID::List::context_t ctx,
                   size_t size_of_list, bool pin_it)
    {
        log_assert(list == pending_list_);
        pending_list_ = nullptr;

        const ID::List list_id =
            cache_.insert(std::move(list), LRU::CacheMode::CACHED, ctx, size_of_list);

        if(pin_it)
            cache_.pin(list_id);

        return list_id;
    }

    void expel_unblessed(std::shared_ptr<LRU::Entry> &&list)
    {
        log_assert(list == pending_list_);
        log_assert(pending_list_ != nullptr);
        list = nullptr;
        pending_list_ = nullptr;
    }

    ID::List get_parent_list_id(ID::List id) const
    {
        if(!id.is_valid())
            return ID::List();

        auto entry = cache_.lookup(id);

        if(entry != nullptr)
        {
            entry = entry->get_parent();

            if(entry != nullptr)
                return entry->get_cache_id();
        }

        return ID::List();
    }

    size_t get_list_depth(ID::List id) const
    {
        if(id.is_valid())
        {
            const auto e(cache_.lookup(id));

            if(e != nullptr)
                return LRU::Entry::depth(*e);
        }

        return 0;
    }

    /*!
     * Look up list with given ID and perform type conversion to type \p T.
     */
    template <typename T>
    std::shared_ptr<T> lookup_list(ID::List id) const
    {
        if(id.is_valid())
            return std::static_pointer_cast<T>(cache_.lookup(id));
        else
            return nullptr;
    }

    template <typename ListType, typename FillerType>
    ID::List enter_child(ID::List list_id, ID::Item item_id,
                         const std::function<bool()> &may_continue,
                         ListError &error)
    {
        if(auto list = lookup_list<ListType>(list_id))
            return list->template enter_child<FillerType>(
                        cache_, default_cache_mode_request_, item_id, may_continue,
                        [this] (ID::List id) { return cache_check_.is_cacheable(id); },
                        [this] (ID::List old_id, ID::List new_id,
                                const EnterChild::SetNewRoot &set_root)
                        {
                            purge_subtree(old_id, new_id, set_root);
                            return new_id;
                        },
                        error);
        else
        {
            error = ListError::INVALID_ID;
            return ID::List();
        }
    }

    template <typename ListType, typename FillerType>
    ID::List enter_child_with_parameters(ID::List list_id, ID::Item item_id,
                                         const char *parameter,
                                         const std::function<bool()> &may_continue,
                                         ListError &error)
    {
        if(auto list = lookup_list<ListType>(list_id))
        {
            return list->template enter_child_with_parameters<FillerType>(cache_, item_id,
                        parameter, may_continue,
                        [this] (ID::List old_id, ID::List new_id,
                                const EnterChild::SetNewRoot &set_root)
                        {
                            purge_subtree(old_id, new_id, set_root);
                            return new_id;
                        },
                        error);
        }
        else
        {
            error = ListError::INVALID_ID;
            return ID::List();
        }
    }

    template <typename T>
    static I18n::String get_dynamic_title(const ListTreeManager &ltm,
                                          ID::List list_id, ID::Item child_item_id)
    {
        const auto list = ltm.lookup_list<const T>(list_id);

        if(list == nullptr)
            return I18n::String(false);

        const auto &item = (*list)[child_item_id];

        std::string temp;
        if(item.get_kind().is_directory())
            item.get_name(temp);

        return I18n::String(false, std::move(temp));
    }

    void announce_root_list(ID::List id);
    void reinsert_list(ID::List &id);

    bool use_list(ID::List id, bool pin_it);

    std::chrono::seconds force_list_into_cache(ID::List list_id, bool force);

    void repin_if_first_is_deepest_pinned_list(ID::List first_id,
                                               ID::List other_id);

    /*!
     * Called when a list was discarded from cache during garbage collection.
     */
    void list_discarded_from_cache(ID::List id);

    /*!
     * Explicitly remove a list and all its sublists from cache.
     *
     * This function is to be called from #ListTreeIface implementations when
     * they need to remove items from a list (e.g., UPnP server lost, directory
     * was deleted on disk, USB pendrive was removed, etc.).
     *
     * \param old_id
     *     Root of the subtree to be removed.
     *
     * \param new_id
     *     What to do with the root ID.
     *     If \p new_id is equal to \p old_id, then only the subtree of
     *     \p old_id is removed and the list referenced by \p old_id remains
     *     intact.
     *     If \p new_id is different from \p old_id, then the list referred to
     *     by \p old_id is removed along with its subtree. In this case,
     *     \p new_id (which may be the invalid ID) is assumed to replace
     *     \p old_id, and the value of \p new_id is passed with the emitted
     *     \c de.tahifi.Lists.Navigation.ListInvalidate D-Bus signal. Note that
     *     in this case \p old_id will be a dangling ID and shall be considered
     *     invalid when this function returns.
     *
     * \param set_root
     *     Function for patching the list entry in the parent list of \p old_id
     *     that referred to \p old_id and should now refer to \p new_id. This
     *     function is bound to be specific to list brokers.
     *
     * \retval #ListTreeManager::PurgeResult::INVALID
     *          ID passed in \p old_id is invalid.
     * \retval #ListTreeManager::PurgeResult::UNTOUCHED
     *          No lists have been purged, nothing happened.
     * \retval #ListTreeManager::PurgeResult::REPLACED_ROOT
     *          No lists have been purged, but a signal about the change of the
     *          root list ID has been sent.
     * \retval #ListTreeManager::PurgeResult::PURGED
     *          More than zero lists have been purged, but root list ID was not
     *          changed.
     * \retval #ListTreeManager::PurgeResult::PURGED_AND_REPLACED
     *          More than zero lists have been purged, and a signal about the
     *          change of the root list ID has been sent.
     */
    PurgeResult purge_subtree(ID::List old_id, ID::List new_id,
                              const EnterChild::SetNewRoot &set_root);

    std::chrono::milliseconds get_gc_expiry_time() const
    {
        return cache_.maximum_age_threshold_;
    }

  private:
    LRU::Cache &cache_;
    Cacheable::CheckIface &cache_check_;
    LRU::CacheModeRequest default_cache_mode_request_;

    std::shared_ptr<LRU::Entry> pending_list_;
};

#endif /* !LISTTREE_MANAGER_HH */
