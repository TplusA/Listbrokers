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

#include "lru.hh"

#include <algorithm>
#include <unordered_map>
#include <ostream>

ID::List LRU::CacheIdGenerator::next(LRU::CacheMode cache_mode,
                                     ID::List::context_t ctx)
{
    log_assert(ctx <= DBUS_LISTS_CONTEXT_ID_MAX);

    uint32_t &next_id_for_context = next_id_[ctx];
    const uint32_t start_point = next_id_for_context;

    do
    {
        const ID::List candidate(make_list_id(next_id_for_context,
                                              cache_mode, ctx));

        if(next_id_for_context < base_id_max_)
            ++next_id_for_context;
        else
        {
            /* ID overflow */
            next_id_for_context = base_id_min_;
        }

        if(is_id_free_(candidate))
            return candidate;
    }
    while(start_point != next_id_for_context);

    return ID::List();
}

LRU::Cache::Cache(size_t memory_hard_upper_limit,
                  size_t count_hard_upper_limit,
                  std::chrono::minutes maximum_age_threshold,
                  unsigned int memory_high_watermark_permil,
                  unsigned int memory_low_watermark_permil,
                  unsigned int count_high_watermark_permil,
                  unsigned int count_low_watermark_permil):
    id_generator_(1, CacheIdGenerator::ID_MAX,
                  [this](ID::List id) -> bool
                  {
                      return all_objects_.find(id) == all_objects_.end();
                  }),
    memory_limits_(memory_hard_upper_limit,
                   memory_high_watermark_permil,
                   memory_low_watermark_permil),
    count_limits_(count_hard_upper_limit,
                  count_high_watermark_permil,
                  count_low_watermark_permil),
    maximum_age_threshold_(maximum_age_threshold),
    root_object_(nullptr),
    oldest_object_(nullptr),
    deepest_youngest_object_(nullptr),
    minimum_required_creation_time_(timebase->now()),
    total_size_(0),
    is_garbage_collector_running_(false)
{}

LRU::Cache::~Cache()
{}

ssize_t LRU::Cache::unlink_objects_on_path_to_root(Entry *entry,
                                                   const Entry *&oldest,
                                                   const Entry *&reconnect_tail_object)
{
    ssize_t depth = -1;

    for(Entry *e = entry; e != nullptr; e = e->get_parent().get())
    {
        if(e == reconnect_tail_object)
            reconnect_tail_object = Entry::AgingList::next_older(*reconnect_tail_object);

        const Entry *younger = Entry::AgingList::unlink(*e);

        if(e == oldest)
        {
            if(younger != nullptr)
                oldest = younger;
            else
                oldest = entry;
        }

        ++depth;
    }

    if(reconnect_tail_object != nullptr)
    {
        /*
         * Follow path up to the root to find the upper-most object with same
         * last time of use---this is where we really need to reconnect. We can
         * find this object by checking the aging list links to figure out if
         * the object is still linked. If it is not, then we just unlinked it
         * above; if it is, then it must have the same last time of use because
         * we started out at the previously deepest youngest node in the tree,
         * so we can be sure that we cannot find any younger node that is still
         * linked (and there cannot be any older node by construction).
         */
        for(const Entry *e = reconnect_tail_object->get_parent().get();
            e != nullptr;
            e = e->get_parent().get())
        {
            if(Entry::AgingList::next_younger(*e) != nullptr ||
               Entry::AgingList::next_older(*e) != nullptr)
            {
                log_assert(Entry::AgeInfo::get_last_use_time(*e) ==
                           Entry::AgeInfo::get_last_use_time(*reconnect_tail_object));
                reconnect_tail_object = e;
            }
            else
                break;
        }
    }

    log_assert(reconnect_tail_object == nullptr ||
               Entry::AgingList::next_younger(*reconnect_tail_object) == nullptr);

    return depth;
}

void LRU::Cache::link_objects_on_path_to_root(Entry *entry,
                                              const Timebase::time_point &now)
{
    for(Entry *e = entry; e != nullptr; e = e->get_parent().get())
    {
        Entry::AgeInfo::set_last_use(*e, now);
        Entry::AgingList::insert_before_parent(*e);
    }
}

bool LRU::Cache::pin_or_unpin_objects_on_path_to_root(const LRU::Cache &cache,
                                                      ID::List id,
                                                      bool pin_them)
{
    if(!id.is_valid())
        return false;

    const auto entry = cache.lookup(id);

    if(entry == nullptr)
        return false;

    for(Entry *e = entry.get(); e != nullptr; e = e->get_parent().get())
        Entry::CacheInfo::set_pin_mode(*e, pin_them);

    return true;
}

ssize_t LRU::Cache::use(const std::shared_ptr<Entry> entry)
{
    log_assert(entry != nullptr);
    log_assert(entry->get_cache_id().is_valid());
    log_assert(lookup(entry->get_cache_id()) != nullptr);

    const Timebase::time_point now = timebase->now();
    log_assert(now >= minimum_required_creation_time_);

    if(now <= minimum_required_creation_time_)
    {
        /*
         * Rare exception: extreme fast usage of objects, not measurable due to
         * limits of clock resolution. We don't need to do anything in this
         * case because we can assume that our object at hand must already be
         * in the correct place within the aging list, and all its parents must
         * have the same age.
         *
         * Note that the "<=" comparison is only done as an act of defensive
         * coding. The \c now time point really can never be smaller than the
         * minimum required creation time.
         */
        return USED_ENTRY_ALREADY_UP_TO_DATE;
    }

    const Entry *reconnect_tail_object =
        (deepest_youngest_object_ != nullptr
         ? deepest_youngest_object_
         : nullptr);

    deepest_youngest_object_ = entry.get();

    ssize_t depth =
        unlink_objects_on_path_to_root(const_cast<Entry *>(deepest_youngest_object_),
                                       oldest_object_, reconnect_tail_object);

    link_objects_on_path_to_root(const_cast<Entry *>(deepest_youngest_object_),
                                 now);

    if(reconnect_tail_object != nullptr)
        Entry::AgingList::join_lists(const_cast<Entry *>(reconnect_tail_object),
                                     const_cast<Entry *>(deepest_youngest_object_));

    log_assert(oldest_object_->is_leaf());

    return depth;
}

ssize_t LRU::Cache::use(ID::List id)
{
    auto obj = LRU::Cache::lookup(id);

    if(obj != nullptr)
        return use(obj);
    else
        return USED_ENTRY_INVALID_ID;
}

bool LRU::Cache::pin(ID::List id)
{
    if(pinned_object_id_ == id)
        return pinned_object_id_.is_valid();

    const bool need_gc = pinned_object_id_.is_valid();

    if(need_gc)
        pin_or_unpin_objects_on_path_to_root(*this, pinned_object_id_, false);

    pinned_object_id_ = id;

    const bool result = id.is_valid()
        ? pin_or_unpin_objects_on_path_to_root(*this, pinned_object_id_, true)
        : true;

    if(!result)
        pinned_object_id_ = ID::List();

    if(!is_garbage_collector_running_ && need_gc)
        gc();

    return pinned_object_id_.is_valid();
}

ID::List LRU::Cache::insert(std::shared_ptr<Entry> &&entry,
                            CacheMode cmode,
                            const ID::List::context_t ctx,
                            size_t size_of_entry)
{
    log_assert(entry != nullptr);

    if(entry->get_cache_id().is_valid())
    {
        BUG("Attempted to insert already cached object into cache");
        return ID::List();
    }

    const Timebase::time_point &entry_last_use(Entry::AgeInfo::get_last_use_time(*entry));

    if(entry_last_use < minimum_required_creation_time_)
    {
        BUG("Attempted to insert outdated object into cache");
        return ID::List();
    }

    const std::shared_ptr<Entry> parent = entry->get_parent();

    if(parent != nullptr)
    {
        if(!parent->get_cache_id().is_valid())
        {
            BUG("Attempted to insert object into cache with unknown parent");
            return ID::List();
        }

        if(entry_last_use < Entry::AgeInfo::get_last_use_time(parent))
        {
            BUG("Attempted to insert object into cache with older parent");
            return ID::List();
        }

        if(use(parent) < 0)
            deepest_youngest_object_ = parent.get();

        Entry::AgingList::add_child(parent);

        log_assert(deepest_youngest_object_ == parent.get());

        if(entry->equal_age(*parent))
            deepest_youngest_object_ = entry.get();
    }
    else
    {
        log_assert(root_object_ == nullptr);
        root_object_ = entry.get();
        deepest_youngest_object_ = entry.get();
    }

    const ID::List id =
        Entry::CacheInfo::set_id(entry, id_generator_.next(cmode, ctx));

    log_assert(all_objects_.find(id) == all_objects_.end());
    all_objects_.insert(std::make_pair(id, entry));

    minimum_required_creation_time_ = Entry::AgeInfo::get_last_use_time(entry);

    if(Entry::AgingList::insert_before(entry, parent))
        oldest_object_ = entry.get();

    log_assert(oldest_object_->is_leaf());

    Entry::CacheInfo::set_size(entry, size_of_entry);
    total_size_ += size_of_entry;

    if(all_objects_.size() == 1)
        notify_first_object_inserted_();

    bool need_gc = false;

    if(memory_limits_.exceeds_soft(total_size_))
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "%s memory limit exceeded by size %zu of new object %u, "
                  "attempting to collect garbage",
                  memory_limits_.exceeds_hard(total_size_) ? "Hard" : "Soft",
                  size_of_entry, id.get_raw_id());
        need_gc = true;
    }

    if(count_limits_.exceeds_soft(all_objects_.size()))
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "%s limit of number of objects exceeded by new object %u, "
                  "attempting to collect garbage",
                  count_limits_.exceeds_hard(all_objects_.size()) ? "Hard" : "Soft",
                  id.get_raw_id());
        need_gc = true;
    }

    if(need_gc)
        notify_garbage_collection_needed_();

    return id;
}

ID::List LRU::Cache::insert_again(std::shared_ptr<Entry> &&entry)
{
    if(entry == nullptr)
        return ID::List();

    const auto old_id = entry->get_cache_id();

    if(all_objects_.erase(old_id) == 0)
        return ID::List();

    Entry::CacheInfo::set_id(entry, id_generator_.next(CacheIdGenerator::get_cache_mode(entry->get_cache_id()),
                                                       entry->get_cache_id().get_context()));

    const auto new_id = entry->get_cache_id();
#ifndef NDEBUG
    const auto inserted =
#endif /* !NDEBUG */
    all_objects_.insert(std::move(std::make_pair(new_id, std::move(entry))));
    log_assert(inserted.second);

    if(old_id == pinned_object_id_)
        pinned_object_id_ = new_id;

    return new_id;
}

std::shared_ptr<LRU::Entry> LRU::Cache::lookup(ID::List entry_id) const
{
    log_assert(entry_id.is_valid());

    auto obj = all_objects_.find(entry_id);

    if(obj != all_objects_.end())
        return obj->second;
    else
        return nullptr;
}

bool LRU::Cache::set_object_size(ID::List entry_id, size_t size_of_entry)
{
    std::shared_ptr<LRU::Entry> obj = lookup(entry_id);

    if(obj == nullptr)
        return false;

    const size_t old_size = Entry::CacheInfo::get_size(obj);
    log_assert(old_size <= total_size_);
    total_size_ -= old_size;

    Entry::CacheInfo::set_size(obj, size_of_entry);
    total_size_ += size_of_entry;

    use(obj);

    if(size_of_entry > old_size && memory_limits_.exceeds_soft(total_size_))
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "%s memory limit exceeded by new size %zu of object %u, "
                  "attempting to collect garbage",
                  memory_limits_.exceeds_hard(total_size_) ? "Hard" : "Soft",
                  size_of_entry, entry_id.get_raw_id());
        gc();
    }

    return true;
}

const LRU::Entry *LRU::Cache::discard(const Entry *const candidate,
                                      bool allow_notifications)
{
    log_assert(oldest_object_ != nullptr);
    log_assert(oldest_object_->is_leaf());
    log_assert(candidate != nullptr);
    log_assert(!candidate->is_pinned());

    auto next_candidate = Entry::AgingList::unlink(*const_cast<Entry *>(candidate));
    if(oldest_object_ == candidate)
        oldest_object_ = next_candidate;

    std::shared_ptr<Entry> parent = candidate->get_parent();

    if(parent != nullptr)
        LRU::Entry::AgingList::del_child(parent);

    if(candidate == deepest_youngest_object_)
        deepest_youngest_object_ = parent.get();

    log_assert(Entry::CacheInfo::get_size(*candidate) <= total_size_);
    total_size_ -= Entry::CacheInfo::get_size(*candidate);

    ID::List removed_object_id = candidate->get_cache_id();

    if(parent != nullptr)
        Entry::CachedObject::obliviate_child(parent, removed_object_id,
                                             candidate);

#ifndef NDEBUG
    size_t removed_count =
#endif /* !NDEBUG */
    all_objects_.erase(removed_object_id);
    log_assert(removed_count == 1);

    if(allow_notifications)
        notify_object_removed_(removed_object_id);

    if(oldest_object_ == nullptr)
    {
        /* deleted the last object, cache is empty now */
        root_object_ = nullptr;

        if(allow_notifications)
            notify_last_object_removed_();
    }

    return next_candidate;
}

class SetFlagUntilReturn
{
  private:
    bool &flag_;

  public:
    SetFlagUntilReturn(const SetFlagUntilReturn &) = delete;
    SetFlagUntilReturn &operator=(const SetFlagUntilReturn &) = delete;

    explicit SetFlagUntilReturn(bool &flag):
        flag_(flag)
    {
        flag_ = true;
    }

    ~SetFlagUntilReturn() { flag_ = false; }
};

std::chrono::seconds LRU::Cache::gc()
{
    log_assert(!is_garbage_collector_running_);

    SetFlagUntilReturn flag_guard(is_garbage_collector_running_);

    const Entry *candidate = oldest_object_;

    while(candidate != nullptr &&
          candidate->get_age() >= maximum_age_threshold_)
    {
        if(!candidate->is_pinned())
            candidate = discard(candidate);
        else
            candidate = Entry::AgingList::next_younger(*candidate);
    }

    if(memory_limits_.exceeds_soft(total_size_) ||
       count_limits_.exceeds_soft(all_objects_.size()))
    {
        /*
         * We should be killing more objects because we are under resource
         * pressure, even if the time for those objects has not come yet.
         */
        while(candidate != nullptr &&
              (!memory_limits_.is_low_enough(total_size_) ||
               !count_limits_.is_low_enough(all_objects_.size())))
        {
            if(candidate->is_pinned())
            {
                candidate = Entry::AgingList::next_younger(*candidate);
                continue;
            }

            if(candidate != deepest_youngest_object_)
                candidate = discard(candidate);
            else
            {
                /*
                 * Too young. This is the hot path the user is likely seeing
                 * right now, so we should only touch it when really needed.
                 */
                if(memory_limits_.exceeds_hard(total_size_) ||
                   count_limits_.exceeds_hard(all_objects_.size()))
                {
                    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                              "Discarding hot object %u (size %sexceeded, count %sexceeded)",
                              candidate->get_cache_id().get_raw_id(),
                              memory_limits_.exceeds_hard(total_size_) ? "" : "not ",
                              count_limits_.exceeds_hard(all_objects_.size()) ? "" : "not ");
                    candidate = discard(candidate);
                }
                else
                    break;
            }
        }
    }

    if(oldest_object_ == nullptr)
    {
        log_assert(root_object_ == nullptr);
        log_assert(deepest_youngest_object_ == nullptr);
        log_assert(all_objects_.empty());

        return std::chrono::seconds::max();
    }

    while(candidate != nullptr && candidate->is_pinned())
        candidate = Entry::AgingList::next_younger(*candidate);

    if(candidate == nullptr)
    {
        /* There are still objects in the cache, but all of them are pinned. */
        for(const auto &obj : all_objects_)
            log_assert(obj.second->is_pinned());

        return std::chrono::seconds::max();
    }

    std::chrono::seconds next_call =
        std::chrono::duration_cast<std::chrono::seconds>(maximum_age_threshold_) -
        std::chrono::duration_cast<std::chrono::seconds>(candidate->get_age());

    if(next_call.count() > 0)
        return next_call;
    else
        return std::chrono::seconds(1);
}

size_t LRU::Cache::count() const
{
    return all_objects_.size();
}

bool LRU::Cache::toposort_for_purge(const std::vector<ID::List>::iterator &kill_list_begin,
                                    const std::vector<ID::List>::iterator &kill_list_end) const
{
    auto first_internal = std::partition(kill_list_begin, kill_list_end,
                                         [this] (const ID::List id)
                                         {
                                             return lookup(id)->is_leaf();
                                         });
    if(first_internal == kill_list_end)
    {
        /* only leaves */
        return true;
    }

    if(!lookup(*kill_list_begin)->is_leaf())
    {
        /* no leaves */
        BUG("Cannot sort for purge because set contains no leaves");
        return false;
    }

    /*
     * This structure stores for each internal node its maximum distance from
     * any leaf in the kill list. These distances are used in a second step for
     * obtaining a list in topologically sorted order.
     */
    std::unordered_map<uint32_t, size_t> nodes_distances;

    for(auto it = first_internal; it != kill_list_end; ++it)
        nodes_distances[(*it).get_raw_id()] = 0;

    for(auto it = kill_list_begin; it != first_internal; ++it)
    {
        const auto &obj = lookup(*it);
        size_t dist = 0;

        for(const Entry *e = obj->get_parent().get();
            e != nullptr;
            e = e->get_parent().get())
        {
            const uint32_t node_id(e->get_cache_id().get_raw_id());

            if(nodes_distances.find(node_id) == nodes_distances.end())
                continue;

            auto &known_dist(nodes_distances[node_id]);

            ++dist;

            if(known_dist < dist)
                known_dist = dist;
            else
                break;
        }
    }

    /*
     * IDs sorted by the distances determined above.
     */
    std::vector<std::pair<size_t, ID::List>> sorted_by_distance;

    std::transform(nodes_distances.begin(), nodes_distances.end(),
        std::back_inserter(sorted_by_distance),
        [] (const auto &it) { return std::make_pair(it.second, ID::List(it.first)); });

    std::sort(sorted_by_distance.begin(), sorted_by_distance.end());

    if(sorted_by_distance.size() != size_t(kill_list_end - first_internal))
    {
        BUG("Cannot sort for purge because kill list is inconsistent");
        return false;
    }

    for(const auto &it : sorted_by_distance)
    {
        *first_internal = it.second;
        ++first_internal;
    }

    return true;
}

void LRU::Cache::purge_entries(const std::vector<ID::List>::const_iterator &kill_list_begin,
                               const std::vector<ID::List>::const_iterator &kill_list_end,
                               bool allow_notifications)
{
    for(auto it = kill_list_begin; it != kill_list_end; ++it)
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Purge entry %u", it->get_raw_id());

        std::shared_ptr<LRU::Entry> obj = lookup(*it);

        if(obj == nullptr)
            BUG("Tried to purge nonexistent entry %u", it->get_raw_id());
        else
        {
            if(obj->is_pinned())
                pin(ID::List());

            discard(obj.get(), allow_notifications);
        }
    }
}

void LRU::Cache::dump_pointers(std::ostream &os, const char *detail) const
{
    os << "===========================\n"
       << "  Cache dump";
    if(detail != nullptr)
        os << " (" << detail << ")";
    os << "\n--------------\n";

    os << "  root " << static_cast<const void *>(root_object_)
       << ", oldest " << static_cast<const void *>(oldest_object_)
       << ", deepest youngest " << static_cast<const void *>(deepest_youngest_object_)
       << "\n";

    os << "  cached objects:\n";

    for(const auto &it : all_objects_)
    {
        const LRU::Entry &obj = *it.second.get();

        os << "    " << it.first.get_raw_id()
           << (it.second->is_pinned() ? '*' : ' ')
           << ((CacheIdGenerator::get_cache_mode(it.first) == LRU::CacheMode::UNCACHED) ? '#' : ' ')
           << " -> " << static_cast<const void *>(&obj)
           << ", age " << obj.get_age().count() << " ms"
           << ", parent " << static_cast<const void *>(it.second->get_parent().get())
           << ", older " << static_cast<const void *>(Entry::AgingList::next_older(obj))
           << ", younger " << static_cast<const void *>(Entry::AgingList::next_younger(obj))
           << ", last use " << Entry::AgeInfo::get_last_use_time(obj).time_since_epoch().count()
           << "\n";
    }

    os << "===========================" << std::endl;
}

void LRU::Cache::self_check() const
{
    static const char fail_message_format[] = "Cache inconsistent: %d";

#define FAIL() \
    do \
    { \
        msg_error(0, LOG_EMERG, fail_message_format, __LINE__); \
        return; \
    } \
    while(0)

#define FAIL_IF(COND) \
    do \
    { \
        if(COND) \
            FAIL(); \
    } \
    while(0)

    FAIL_IF(!(root_object_ == nullptr && oldest_object_ == nullptr &&
              deepest_youngest_object_ == nullptr) &&
            !(root_object_ != nullptr && oldest_object_ != nullptr &&
              deepest_youngest_object_ != nullptr));

    size_t number_of_root_objects = 0;
    size_t number_of_oldest_objects = 0;
    size_t number_of_deepest_youngest_objects = 0;
    size_t number_of_pinned_objects = 0;
    size_t number_of_children = 0;

    for(const auto it : all_objects_)
    {
        const std::shared_ptr<LRU::Entry> obj = it.second;

        if(obj.get() == root_object_)
            ++number_of_root_objects;

        if(obj.get() == oldest_object_)
            ++number_of_oldest_objects;

        if(obj.get() == deepest_youngest_object_)
            ++number_of_deepest_youngest_objects;

        if(obj->get_cache_id() == pinned_object_id_)
            ++number_of_pinned_objects;

        if(obj->is_pinned())
        {
            for(const Entry *e = obj->get_parent().get();
                e != nullptr;
                e = e->get_parent().get())
            {
                FAIL_IF(!e->is_pinned());
            }
        }

        FAIL_IF(obj == nullptr);
        FAIL_IF(obj->get_cache_id() != it.first);
        FAIL_IF(obj->get_parent() == obj);

        const size_t children =
            std::count_if(all_objects_.begin(), all_objects_.end(),
                [&obj] (const auto &o) { return o.second->get_parent() == obj; });
        number_of_children += children;

        FAIL_IF(children != obj->get_number_of_children());
        FAIL_IF(children == 0 && !obj->is_leaf());
        FAIL_IF(children != 0 && obj->is_leaf());
    }

    FAIL_IF(root_object_ == nullptr && number_of_children != 0);
    FAIL_IF(root_object_ != nullptr && number_of_children + 1 != count());
    FAIL_IF(root_object_ == nullptr && number_of_root_objects != 0);
    FAIL_IF(root_object_ != nullptr && number_of_root_objects != 1);
    FAIL_IF(oldest_object_ == nullptr && number_of_oldest_objects != 0);
    FAIL_IF(oldest_object_ != nullptr && number_of_oldest_objects != 1);
    FAIL_IF(deepest_youngest_object_ == nullptr && number_of_deepest_youngest_objects != 0);
    FAIL_IF(deepest_youngest_object_ != nullptr && number_of_deepest_youngest_objects != 1);
    FAIL_IF(!pinned_object_id_.is_valid() && number_of_pinned_objects != 0);
    FAIL_IF(pinned_object_id_.is_valid() && number_of_pinned_objects != 1);

    if(root_object_ != nullptr)
    {
        FAIL_IF(root_object_->get_parent() != nullptr);
        FAIL_IF(Entry::AgingList::next_younger(*root_object_) != nullptr);
        FAIL_IF(Entry::AgingList::next_older(*oldest_object_) != nullptr);

        auto prev_age = oldest_object_->get_age();

        for(auto it = begin(); it != end(); ++it)
        {
            FAIL_IF((*it).get_age() > prev_age);
            prev_age = (*it).get_age();
        }

        for(auto it = rbegin(); it != rend(); ++it)
        {
            FAIL_IF((*it).get_age() < prev_age);
            prev_age = (*it).get_age();
        }

        const auto youngest_age = deepest_youngest_object_->get_age();

        for(const Entry *e = deepest_youngest_object_->get_parent().get();
            e != nullptr;
            e = e->get_parent().get())
        {
            FAIL_IF(e->get_age() != youngest_age);
        }
    }

#undef FAIL

}

void LRU::AgingListEntry::insert_oldest(Entry *oldest, Entry *younger_object,
                                        AgingListEntry &younger_aging_list_data)
{
    log_assert(younger_aging_list_data.older_ == nullptr);

    this->older_ = nullptr;
    this->younger_ = younger_object;

    younger_aging_list_data.older_ = oldest;
}

void LRU::AgingListEntry::insert_between(Entry *center, AgingListEntry &older,
                                         AgingListEntry &younger)
{
    this->older_ = younger.older_;
    this->younger_ = older.younger_;

    older.younger_ = center;
    younger.older_ = center;
}

void LRU::AgingListEntry::unlink(AgingListEntry *older, AgingListEntry *younger)
{
    if(older != nullptr)
        older->younger_ = this->younger_;

    if(younger != nullptr)
        younger->older_ = this->older_;

    this->older_ = nullptr;
    this->younger_ = nullptr;
}

void LRU::AgingListEntry::join(Entry *tail, Entry *head,
                               AgingListEntry &head_aging_list_data)
{
    head_aging_list_data.older_ = tail;
    this->younger_ = head;
}

size_t LRU::Entry::depth(const Entry &entry)
{
    size_t d = 0;

    for(const Entry *e = &entry; e != nullptr; e = e->get_parent().get())
        ++d;

    return d;
}

void LRU::Entry::enumerate_tree_of_sublists(const LRU::Cache &cache,
                                            std::vector<ID::List> &nodes,
                                            bool append_to_nodes) const
{
    if(!append_to_nodes)
        nodes.clear();

    nodes.push_back(get_cache_id());

    for(size_t next_unprocessed = nodes.size() - 1;
        next_unprocessed < nodes.size();
        ++next_unprocessed)
    {
        const auto &list = cache.lookup(nodes[next_unprocessed]);

        list->enumerate_direct_sublists(cache, nodes);
    }
}

bool LRU::Entry::insert_before(const Entry *const younger_object) const
{
    if(younger_object == nullptr)
        return true;

    const Entry *older_object = younger_object->aging_list_data_.next_older();

    if(older_object != nullptr)
    {
        log_assert(older_object->aging_list_data_.next_younger() == younger_object);

        const_cast<Entry *>(this)->aging_list_data_.insert_between(const_cast<Entry *>(this),
                                                                   const_cast<Entry *>(older_object)->aging_list_data_,
                                                                   const_cast<Entry *>(younger_object)->aging_list_data_);
        return false;
    }
    else
    {
        const_cast<Entry *>(this)->aging_list_data_.insert_oldest(const_cast<Entry *>(this),
                                                                  const_cast<Entry *>(younger_object),
                                                                  const_cast<Entry *>(younger_object)->aging_list_data_);
        return true;
    }
}

void LRU::Entry::append(const Entry *const head) const
{
    log_assert(head != nullptr);
    log_assert(head->aging_list_data_.next_older() == nullptr);
    log_assert(this->aging_list_data_.next_younger() == nullptr);

    const_cast<Entry *>(this)->aging_list_data_.join(const_cast<Entry *>(this),
                                                     const_cast<Entry *>(head),
                                                     const_cast<Entry *>(head)->aging_list_data_);
}

const LRU::Entry *LRU::Entry::unlink_from_aging_list() const
{
    const Entry *older_object = aging_list_data_.next_older();
    const Entry *younger_object = aging_list_data_.next_younger();

    AgingListEntry *const older_list_data =
        (older_object
         ? &const_cast<Entry *>(older_object)->aging_list_data_
         : nullptr);

    AgingListEntry *const younger_list_data =
        (younger_object
         ? &const_cast<Entry *>(younger_object)->aging_list_data_
         : nullptr);

    const_cast<Entry *>(this)->aging_list_data_.unlink(older_list_data,
                                                       younger_list_data);

    return younger_object;
}
