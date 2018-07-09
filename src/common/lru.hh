/*
 * Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
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

#ifndef LRU_HH
#define LRU_HH

#include <memory>
#include <map>
#include <vector>
#include <ostream>
#include <functional>

#include "idtypes.hh"
#include "timebase.hh"
#include "messages.h"

/*!
 * \addtogroup lru_cache Least recently used object cache
 */
/*!@{*/

namespace LRU
{

extern Timebase *timebase;

enum class CacheMode
{
    CACHED,
    UNCACHED,
};

enum class CacheModeRequest
{
    CACHED,
    UNCACHED,
    AUTO,
};

static inline CacheMode to_cache_mode(const CacheModeRequest req)
{
    return req == CacheModeRequest::AUTO ? CacheMode::CACHED : CacheMode(req);
}

class Entry;
class Cache;

/*!
 * Data about the cached object relevant for the object cache.
 *
 * Each cached object contains an instance of this class as a member. This
 * class exists to keep cache object class interfaces lean.
 */
class CacheMetaData
{
  private:
    ID::List id_;
    size_t object_size_;
    bool is_pinned_;

  public:
    CacheMetaData(const CacheMetaData &) = delete;
    CacheMetaData &operator=(const CacheMetaData &) = delete;

    explicit CacheMetaData():
        id_(0),
        object_size_(0),
        is_pinned_(false)
    {}

    ID::List get_id() const
    {
        return id_;
    }

    size_t get_size() const
    {
        return object_size_;
    }

    bool is_pinned() const
    {
        return is_pinned_;
    }

  private:
    ID::List set_id(ID::List id)
    {
        id_ = id;
        return id_;
    }

    void set_size(size_t object_size)
    {
        object_size_ = object_size;
    }

    void set_pin_mode(bool pin_it)
    {
        is_pinned_ = pin_it;
    }

    /* for setting the object ID at insertion time and setting object size */
    friend class Entry;
};

/*!
 * Linkage of aging list and relevant time point data.
 *
 * Objects are linked in the aging list, a list of objects of descending age.
 * The head of this list is the oldest object and to first one to be removed
 * from the cache when the cache policies require cache oblivion.
 *
 * Each cached object contains an instance of this class as a member. The
 * interfaces of cached objects are kept lean by this, and all data relevant to
 * linked list management are bundled in this place.
 */
class AgingListEntry
{
  public:
    AgingListEntry(const AgingListEntry &) = delete;
    AgingListEntry &operator=(const AgingListEntry &) = delete;

    explicit AgingListEntry(const Timebase::time_point &created,
                            const Entry *younger, const Entry *older):
        last_used_(created),
        younger_(younger),
        older_(older)
    {}

    /*!
     * Return age of entry (live information based on current time).
     */
    std::chrono::milliseconds get_age() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(timebase->now() - last_used_);
    }

    const Timebase::time_point &get_last_use_time() const
    {
        return last_used_;
    }

    void last_use_at(const Timebase::time_point &tp)
    {
        log_assert(tp <= timebase->now());
        last_used_ = tp;
    }

    const Entry *next_younger() const
    {
        return younger_;
    }

    const Entry *next_older() const
    {
        return older_;
    }

    void insert_oldest(Entry *oldest, Entry *younger_object,
                       AgingListEntry &younger_aging_list_data);

    void insert_between(Entry *center, AgingListEntry &older,
                        AgingListEntry &younger);

    void unlink(AgingListEntry *older, AgingListEntry *younger);

    void join(Entry *tail, Entry *head, AgingListEntry &head_aging_list_data);

  private:
    Timebase::time_point last_used_;
    const Entry *younger_;
    const Entry *older_;
};

/*!
 * One cached entry in the LRU cache.
 *
 * Every entry has a link to one parent entry and a counter for the number of
 * child objects. There is no efficient way to traverse the tree downwards,
 * only one for following the path to the root node. Objects also have an
 * identifier and directly store their aging list entry data.
 *
 * \note
 *     This is just a base class for managing the cache meta data. Other
 *     classes must derive from this class to fill it with content.
 */
class Entry
{
  private:
    const std::shared_ptr<Entry> parent_;
    size_t children_count_;

    CacheMetaData cache_data_;
    AgingListEntry aging_list_data_;

  protected:
    explicit Entry(const std::shared_ptr<Entry> &parent):
        parent_(parent),
        children_count_(0),
        aging_list_data_(timebase->now(), nullptr, nullptr)
    {}

  public:
    Entry(const Entry &) = delete;
    Entry &operator=(const Entry &) = delete;

    virtual ~Entry() {}

    ID::List get_cache_id() const
    {
        return cache_data_.get_id();
    }

    const std::shared_ptr<Entry> &get_parent() const
    {
        return parent_;
    }

    std::chrono::milliseconds get_age() const
    {
        return aging_list_data_.get_age();
    }

    bool equal_age(const Entry &e) const
    {
        return aging_list_data_.get_last_use_time() == e.aging_list_data_.get_last_use_time();
    }

    /*!
     * Depth of given cached object in the tree of objects.
     *
     * This function traverses the chain of parent links from \p entry all the
     * way up to the root node and reports back the number of nodes seen on the
     * way, including node \p entry and the root node. Thus, the minimum value
     * this function will return is 1, indicating that the function has been
     * called for the root node.
     */
    static size_t depth(const Entry &entry);

    /*!
     * Whether or not the cached object has child objects.
     *
     * This information has nothing to do with the list content and its logical
     * directory structure. It is possible for a cached object to be a leaf,
     * yet contain only items representing directories ("directory" is a
     * higher-level concept the LRU cache is not concerned with). As long as
     * these directories have not been accessed, the #LRU::Entry object is just
     * a leaf from the cache's point of view.
     */
    bool is_leaf() const
    {
        return children_count_ == 0;
    }

    size_t get_number_of_children() const { return children_count_; }

    bool is_pinned() const
    {
        return cache_data_.is_pinned();
    }

    /*!
     * Collect the list IDs of all sublists recursively referenced by this
     * entry, including this entry.
     *
     * \param cache
     *     Cache the list IDs refer to.
     *
     * \param nodes
     *     The list of IDs is expected to be returned here. Since the object
     *     this function is called for must also be part of this list, it is
     *     an error to return an empty list.
     *
     * \param append_to_nodes
     *     Whether or not to clear \p nodes. If true, the original contents of
     *     \p nodes will be kept and new content is appended to the ilst,
     *     otherwise the list's original content will be replaced.
     */
    virtual void enumerate_tree_of_sublists(const Cache &cache,
                                            std::vector<ID::List> &nodes,
                                            bool append_to_nodes = false) const;

    /*!
     * Collect the list IDs of sublists referenced directly by this entry,
     * excluding this entry.
     *
     * \param cache
     *     Cache the list IDs refer to.
     *
     * \param nodes
     *     The list of IDs is expected to be returned here. IDs are to be
     *     \e appended to the list. The function shall \e not clear the list.
     */
    virtual void enumerate_direct_sublists(const Cache &cache,
                                           std::vector<ID::List> &nodes) const = 0;

  private:
    /*!
     * Insert this object in front of another object.
     *
     * \note
     * Logically, the #LRU::Entry objects we are dealing with in here are const
     * because we don't modify the caller's payload---we only update meta data.
     * So we use \c const_cast a few times to keep up the caller's impression
     * of having a constant object.
     */
    bool insert_before(const Entry *const younger_object) const;

    /*!
     * Append another list to tail element.
     *
     * The object this function is called for must be the tail (youngest)
     * object of the list the other list should be appended to.
     *
     * \param head
     *     The head of the second list that should be connected to the tail
     *     element of the first list (\c this).
     */
    void append(const Entry *const head) const;

    /*!
     * Unlink \c this from the cache's aging list.
     *
     * This is a helper function called from the aging list management code. It
     * cannot be called directly, but friends of #LRU::Entry::AgingList may
     * call it through #LRU::Entry::AgingList::unlink().
     */
    const Entry *unlink_from_aging_list() const;

    /*!
     * Increase child counter.
     */
    void add_child()
    {
        ++children_count_;
    }

    /*!
     * Decrease child counter.
     */
    void del_child()
    {
        log_assert(children_count_ > 0);
        --children_count_;
    }

    /*!
     * Called to notify that a child list was discarded from cache.
     *
     * When this function is called, the cached object with ID \p child_id has
     * been discarded, and that object was the child of the object for which
     * this function is called. The function should take measures to remove any
     * references to \p child_id because that ID is not valid anymore, but
     * other than that it shall not assume that any content has changed. The
     * child object may have vanished for the time being, but it could be
     * materialized again at some later point (with a new ID).
     */
    virtual void obliviate_child(ID::List child_id, const Entry *child) = 0;

  public:
    class AgingList
    {
        static const Entry *next_older(const Entry &entry)
        {
            return entry.aging_list_data_.next_older();
        }

        static const Entry *next_younger(const Entry &entry)
        {
            return entry.aging_list_data_.next_younger();
        }

        static void add_child(const std::shared_ptr<Entry> &entry)
        {
            entry->add_child();
        }

        static void del_child(const std::shared_ptr<Entry> &entry)
        {
            entry->del_child();
        }

        static bool insert_before(const std::shared_ptr<Entry> &new_entry,
                                  const std::shared_ptr<Entry> &existing_entry)
        {
            return new_entry->insert_before(existing_entry.get());
        }

        static void join_lists(Entry *tail, Entry *head)
        {
            tail->append(head);
        }

        static void insert_before_parent(Entry &entry)
        {
            if(entry.get_parent() != nullptr)
                entry.append(entry.get_parent().get());
        }

        static const Entry *unlink(Entry &entry)
        {
            return entry.unlink_from_aging_list();
        }

        /* for iterator implementation and aging list management */
        friend class Cache;
    };

    class CachedObject
    {
        static void obliviate_child(const std::shared_ptr<Entry> &entry,
                                    ID::List child_id, const Entry *child)
        {
            entry->obliviate_child(child_id, child);
        }

        /* for telling an object that a child objects has been discarded */
        friend class Cache;
    };

    class AgeInfo
    {
        static void set_last_use(Entry &entry, const Timebase::time_point &tp)
        {
            entry.aging_list_data_.last_use_at(tp);
        }

        static const Timebase::time_point &get_last_use_time(const Entry &entry)
        {
            return entry.aging_list_data_.get_last_use_time();
        }

        static const Timebase::time_point &get_last_use_time(const std::shared_ptr<Entry> &entry)
        {
            return entry->aging_list_data_.get_last_use_time();
        }

        /* for age information updates */
        friend class Cache;
    };

    class CacheInfo
    {
        static ID::List set_id(const std::shared_ptr<Entry> &entry, ID::List id)
        {
            return entry->cache_data_.set_id(id);
        }

        static size_t get_size(const std::shared_ptr<Entry> &entry)
        {
            return entry->cache_data_.get_size();
        }

        static size_t get_size(const Entry &entry)
        {
            return entry.cache_data_.get_size();
        }

        static void set_size(const std::shared_ptr<Entry> &entry, size_t object_size)
        {
            entry->cache_data_.set_size(object_size);
        }

        static void set_pin_mode(Entry &entry, bool pin_it)
        {
            entry.cache_data_.set_pin_mode(pin_it);
        }

        /* for setting the object ID at insertion time and setting object size */
        friend class Cache;
    };
};

/*!
 * Cache size limits.
 *
 * There are three unitless values stored:
 * 1. a hard upper limit (not to be exceeded);
 * 2. a soft upper limit (may be exceeded if necessary); and
 * 3. a lower limit.
 *
 * When exceeding the soft upper limit (see #LRU::CacheLimits::exceeds_soft()),
 * the cache should start discarding objects until the value in question drops
 * below the lower limit (see #LRU::CacheLimits::is_low_enough()). Exceeding
 * the hard upper limit must at least trigger the emission of some log message
 * and should trigger discarding of cached objects as soon as possible.
 *
 * Careful tuning of these limits should keep the memory consumpion low enough
 * to keep the system stable and performant.
 */
class CacheLimits
{
  private:
    const size_t hard_upper_limit_;
    const size_t high_watermark_;
    const size_t low_watermark_;

  public:
    CacheLimits(const CacheLimits &) = delete;
    CacheLimits &operator=(const CacheLimits &) = delete;

    explicit CacheLimits(size_t hard_upper_limit, unsigned int high_permil,
                         unsigned int low_permil):
        hard_upper_limit_(hard_upper_limit),
        high_watermark_((uint64_t(hard_upper_limit) * uint64_t(high_permil) + 500UL) / 1000UL),
        low_watermark_((uint64_t(hard_upper_limit) * uint64_t(low_permil) + 500UL) / 1000UL)
    {
        log_assert(hard_upper_limit > 0U);
        log_assert(high_permil <= 1000U);
        log_assert(low_permil <= 1000U);
        log_assert(high_permil > low_permil);

        log_assert(hard_upper_limit_ >= high_watermark_);
        log_assert(high_watermark_ > low_watermark_);
    }

    bool exceeds_soft(size_t value) const
    {
        return value > high_watermark_;
    }

    bool exceeds_hard(size_t value) const
    {
        return value > hard_upper_limit_;
    }

    bool is_low_enough(size_t value) const
    {
        return value < low_watermark_;
    }
};

/*!
 * Helper class for obtaining a usable ID.
 */
class CacheIdGenerator
{
  public:
    static constexpr const uint32_t ID_MAX = ID::List::VALUE_MASK;

  private:
    const uint32_t base_id_min_;
    const uint32_t base_id_max_;
    const std::function<bool(ID::List)> is_id_free_;

    /*!
     * Next candidate for an ID, further checked in #next() member function.
     */
    std::array<uint32_t, DBUS_LISTS_CONTEXT_ID_MAX + 1> next_id_;

  public:
    CacheIdGenerator(const CacheIdGenerator &) = delete;
    CacheIdGenerator &operator=(const CacheIdGenerator &) = delete;

    explicit CacheIdGenerator(const uint32_t base_id_min,
                              const uint32_t base_id_max,
                              std::function<bool(ID::List)> &&is_id_free):
        base_id_min_(base_id_min),
        base_id_max_(base_id_max),
        is_id_free_(is_id_free)
    {
        next_id_.fill(base_id_min);
    }

    /*!
     * Generate next list ID for given context.
     *
     * \param cache_mode
     *     Whether or not the object shall remain in cache on garbage
     *     collection. Uncached objects always get garbage collected if they
     *     are not pinned, cached objects remain in cache until they expire.
     *
     * \param ctx
     *     List context ID between #DBUS_LISTS_CONTEXT_ID_MIN and
     *     #DBUS_LISTS_CONTEXT_ID_MAX, including boundaries.
     *
     * \returns
     *     A new list ID, or the invalid ID if there are no free list IDs left.
     */
    ID::List next(CacheMode cache_mode, ID::List::context_t ctx);

    /*!
     * Figure out cache mode for an entry ID.
     */
    static CacheMode get_cache_mode(ID::List entry_id)
    {
        return (entry_id.get_nocache_bit()
                ? CacheMode::UNCACHED
                : CacheMode::CACHED);
    }

  private:
    static ID::List make_list_id(uint32_t raw_id, LRU::CacheMode cache_mode, ID::List::context_t ctx)
    {
        return ID::List(raw_id |
                        (ctx << DBUS_LISTS_CONTEXT_ID_SHIFT) |
                        ((cache_mode == CacheMode::UNCACHED) ? ID::List::NOCACHE_BIT : 0));
    }
};

/*!
 * An LRU (least recently used) object cache implementation.
 *
 * This cache stores objects for fast retrieval once they have been
 * constructed. Objects are identified by numerical IDs as assigned by the
 * cache object at insertion time. The objects are organized as a tree by
 * parent pointers.
 *
 * Objects managed by the cache are \c shared_ptr objects pointing to objects
 * of type(s) derived from class #LRU::Entry. Allocation and deallocation of
 * these objects are managed outside of the cache, but the use of smart
 * pointers makes it safe and easy. The cache expects to manage trees of
 * objects and therefore requires all objects (except for the first, root
 * object) to have a parent upon insertion. Further details on intended use
 * are best found out by reading the unit tests (see groups #lru_cache_tests
 * and #lru_cacheentry_tests).
 *
 * There are certain limits concerning the RAM consumpion and the total number
 * of cached objects. If any of these limits are exceeded, then objects are
 * discarded from the cache until \e all limited values drop below their
 * respective lower limits, not only the value that just exceeded its limit.
 *
 * In addition to these non-temporal limits, another time-based limit is used
 * to restrict the <em>maximum age</em> of objects. An object's age at time
 * point \e t is the duration of time between \e t and the time the object has
 * been used last.
 *
 * \par Candidate selection algorithm
 *
 * When exceeding a limit, candidates for discarding are selected based on
 * their time of most recent use. Those objects whose time point of most recent
 * use lies farthest away in the past (making them the least recently used
 * objects) are discarded first.
 *
 * Since we are working on data organized as a tree, the discarding algorithm
 * takes advantage of this as follows:
 * - Making use of objects by looking them up, explicitly marking them as
 *   recently used (see #LRU::Cache::use()), or using it in some other way
 *   always also marks all parent objects as recently used, all the way up to
 *   the root object. This ensures that the age of objects decreases
 *   monotonically when tracing back any path up to the root object, with
 *   oldest objects at the deepest nodes and the youngest objects near the
 *   root.
 * - Finding the oldest objects means therefore enumerating the leaves and
 *   sorting them by age.
 * - When discarding the oldest object (which is always a leaf), all objects on
 *   the path back to the root up to and not including the first object whose
 *   age is smaller than or (important!) equal to the discarded object are
 *   discarded, regardless of limits. This means that more objects may be
 *   removed than necessary to keep values within their limits, but since all
 *   objects discarded this way are of the same age, they are as well all
 *   equally unlikely to be of interest any time soon. Objects with same age as
 *   the child objects must not be discarded because there might be other
 *   subtrees with the same timestamp because of very fast object navigation
 *   and limitations in time resolution.
 *
 * \par Implementation details
 *
 * There are three basic operations that need to be supported:
 * - <b>\c INSERT-NEW</b>     to insert a new cached object;
 * - <b>\c DISCARD-OLDEST</b> to discard the oldest cached object; and
 * - <b>\c USE-OBJECT</b>     to mark an object (and its parent objects) as
 *                            most recently used.
 *
 * <small>
 * (Operations \c INSERT-NEW and \c DISCARD-OLDEST can be mapped directly to
 * the operations of a standard binary max heap, but \c USE-OBJECT requires
 * removal of random access from the heap, something which is not possible in
 * an efficient way with a binary heap. So we need something else.)
 * </small>
 *
 * Cached objects are linked by parent pointers and thus form a tree structure.
 * There are no child pointers, but each cached object has a counter that tells
 * us how many children it has so that we are able to tell internal nodes from
 * leaves. In addition, all cached objects at are also linked by a doubly
 * linked list so that the head of this list points to the oldest object, and
 * the list tail points to the youngest one. A link from one object \e A to the
 * next object \e B means that \e A is at least as old as \e B, and the link
 * back from \e B to \e A equivalently means that object \e B is not older than
 * \e B; note, however, that \e A and \e B could still be of equal age.
 *
 * We call this doubly linked list of cached objects of monotonically
 * decreasing age the <b>aging list</b>. Objects remain in this list until they
 * become too old and need to die and be discarded, with the next candidate at
 * the head of the list.
 *
 * The cache maintains three pointers to objects in the tree: the root object
 * (\e root), the oldest object (\e oldest, which is the head of the aging
 * list), and the youngest objects farthest away from the root
 * (\e first_youngest, may also be the root itself).
 *
 * <b>\c INSERT-NEW</b>:
 * The main work for inserting a new cached object, say \e C, is done by
 * \c USE-OBJECT.
 * 1. The soon-to-be parent of \e C, say \e P, is marked as used using the
 *    \c USE-OBJECT operation (\e C must be a leaf object, otherwise it would
 *    have been inserted before and would only have to be updated). This moves
 *    \c P near the end of the aging list and updates its age.
 * 2. The parent pointer of \e C is set to \e P.
 * 3. The child counter of \e C is set to 0.
 * 4. The child counter of \e P is increased by one.
 * 5. Insert \e C into the aging list in front of \e P.
 * 6. The \e first_youngest pointer is set to point to \e C.
 *
 * Except for the \c USE-OBJECT operations, all of the above steps take O(1)
 * time, leading to an algorithm dominated by the efficiency of \c USE-OBJECT.
 *
 * <b>\c USE-OBJECT</b>:
 * Using an object \e C means changing its age and the age of all parent nodes
 * up to the root object so that it becomes the youngest object. (The algorithm
 * does nothing in case the new age is equal to the object's old age, as could
 * happen in case of low clock resolution.). Therefore, by construction, the
 * subtree below any object contains only child objects which are not younger
 * than the object at the root of that subtree, and therefore the root node of
 * the whole cache always contains a youngest object (there might be several).
 *
 * Now, there are two things to do:
 * 1. the age of all objects on the path to the root must be changed; and
 * 2. the objects on the path to the root have to be relinked in the aging list
 *    because their age has changed.
 *
 * For an efficient solution, we need to avoid checking all objects in the
 * cache and must touch only those objects which are directly affected by the
 * change.
 *
 * Indeed, in a cache with \e n objects, changing object ages is possible by
 * following the path to the root node and updating the object ages and their
 * links in O(n) amortized worst case time (happens in a tree consisting of
 * nodes with only single or zero child nodes) and O(log(n)) expected time
 * (assuming a balanced tree).
 *
 * To enable efficient execution of the \c DISCARD-OLDEST operation, the
 * relinking of the aging list works in a way that avoids links from any object
 * to other ones with same age contained in its subtree. The aging list
 * therefore always enumerates objects of equal age from bottom to top.
 *
 * Our algorithm works in two phases. The first phase unlinks all objects on
 * the path from \e C to the root (including the root) from the aging list. The
 * second phase updates the time of last use for all objects on the path and
 * links them into a partial aging list that is finally appended to the end of
 * the previously existing aging list (which was, not considering special
 * cases, essentially cut off before \e first_youngest).
 *
 * While traversing the path, the \e oldest pointer and the end of the aging
 * list need to be considered. If they lie on the path, they are changed so
 * that they lie outside the path (in O(1) time).
 *
 * Altogether, there are only up to three traversals from objects to the root
 * (two obvious traversals in the phases, plus one for finding the correct tail
 * element of the aging list to which the new partial list has to be appended),
 * leading to the amortized time efficiency stated before.
 *
 * <b>\c DISCARD-OLDEST</b>:
 * This operation makes sure that after discarding the oldest object the head
 * element of the aging list is always a leaf. Therefore, the oldest object in
 * the cache is always trivially found in O(1) time. Let \e D be the object to
 * be discarded and let \e P be its parent. Further, let \e C be the object
 * which comes next in the aging list.
 *
 * The algorithm performs the following steps:
 * 1. Object \e D gets removed from the aging list, causing \e C to become the
 *    new head of the aging list.
 * 2. The child counter of \e P is decreased by 1.
 * 3. If \e first_youngest points to \e D, then set \e first_youngest to \e P.
 * 4. Object \e D gets discarded.
 * 5. By construction, \e C is guaranteed to be a leaf object, and the
 *    algorithm terminates.
 *
 * Each of the steps above take O(1) time, leading to a total amortized O(1)
 * time algorithm. Note that this only works because \c INSERT-NEW and \c
 * USE-OBJECT already do the work required to keep the aging list in proper
 * shape.
 */
class Cache
{
  private:
    CacheIdGenerator id_generator_;

    const CacheLimits memory_limits_;
    const CacheLimits count_limits_;

  public:
    /*!
     * During garbage collection, discard objects at least this old.
     */
    const std::chrono::minutes maximum_age_threshold_;

  private:
    /*!
     * All objects in the cache, organized by list ID.
     *
     * This container is the main structure of this class. The plain pointers
     * to #LRU::Entry used by this class are all contained in
     * #LRU::Cache::all_objects_. All public interfaces deal with smart
     * pointers (\c std::shared_ptr), so our internal plain pointers are safe.
     */
    std::map<ID::List, std::shared_ptr<Entry>> all_objects_;

    /*!
     * The root object in the tree hierarchy.
     *
     * By construction, this object is also also the youngest object in the
     * cache.
     */
    const Entry *root_object_;

    /*!
     * The oldest object in the cache (also head of the aging list).
     */
    const Entry *oldest_object_;

    /*!
     * Of all youngest objects, this is the one farthest away from the root
     * object in the tree hierarchy.
     */
    const Entry *deepest_youngest_object_;

    /*!
     * Never garbage collect this object or any of the objects on the path to
     * the root object.
     *
     * May also be \c nullptr in which case all objects will be garbage
     * collected after some time, resulting in an empty cache.
     */
    ID::List pinned_object_id_;

    /*!
     * Objects created before this time may not be inserted into the cache.
     *
     * Essentially, this is simply the time of last use of the youngest object
     * in the cache.
     *
     * To keep the implementation efficient and memory-conserving, we cannot
     * afford to allow insertion of arbitrary objects. By only allowing
     * insertion of objects which are not older than the youngest one,
     * inserting a new object is a matter of using and updating the
     * #LRU::Cache::deepest_youngest_object_ and #LRU::Cache::root_object_
     * pointers.
     */
    Timebase::time_point minimum_required_creation_time_;

    /*!
     * Cumulated sum of sizes of all objects in the cache.
     */
    size_t total_size_;

    /*!
     * True while the garbage collector is running.
     *
     * This flag is used to avoid the garbage collector from calling itself.
     * There are several indirections that may lead to this situation, and it
     * must be avoided.
     */
    bool is_garbage_collector_running_;

    std::function<void()> notify_first_object_inserted_;
    std::function<void()> notify_garbage_collection_needed_;
    std::function<void(ID::List)> notify_object_removed_;
    std::function<void()> notify_last_object_removed_;

  public:
    Cache(const Cache &) = delete;
    Cache &operator=(const Cache &) = delete;

    /*!
     * Construct a cache object with certain limits.
     *
     * Note that #LRU::Cache::set_callbacks() must be called before the cache
     * can be used. This extra function is separated from the constructor to
     * enable breaking circular dependencies between the cache object and the
     * callbacks it is calling.
     *
     * \param memory_hard_upper_limit
     * \param memory_high_watermark_permil
     * \param memory_low_watermark_permil
     *     These are the limits for the total memory consumption by the cached
     *     objects. Consuming all RAM (or more) than available is bad in any
     *     system, so we are looking to avoid this situation.
     *
     * \param count_hard_upper_limit
     * \param count_high_watermark_permil
     * \param count_low_watermark_permil
     *     These are the limits for the total number of cached objects. A
     *     limitation on the number of objects is useful for avoiding a ton of
     *     micro-objects that may not consume much memory, but otherwise litter
     *     the cache by their sheer amount and thus cause an overall
     *     performance drain.
     *
     * \param maximum_age_threshold
     *     Defines the maximum duration any object may reside in cache after
     *     its last use. Any objects not younger than this are discarded during
     *     garbage collection. This allows the cache size to drop to size 0
     *     after some time without use, freeing RAM for use by other processes
     *     and avoiding keeping very old objects hanging around forever.
     */
    explicit Cache(size_t memory_hard_upper_limit,
                   size_t count_hard_upper_limit,
                   std::chrono::minutes maximum_age_threshold,
                   unsigned int memory_high_watermark_permil = 900,
                   unsigned int memory_low_watermark_permil = 400,
                   unsigned int count_high_watermark_permil = 900,
                   unsigned int count_low_watermark_permil = 400);

    ~Cache();

    /*!
     * Set callbacks that are called on certain occasions.
     *
     * This function must be called before using the #LRU::Cache object.
     * All callbacks must be defined, possibly just as empty functions.
     *
     * \param notify_first_object_inserted
     *     Called after the first object has been inserted into the cache.
     *
     * \param notify_garbage_collection_needed
     *     Called when garbage collection should be run because some configured
     *     limit has been exceeded.
     *
     * \param notify_object_removed
     *     Called for each object removed from the cache right after it has
     *     been removed. The list ID passed to this function is therefore not
     *     bound to an object anymore when the function is called.
     *
     * \param notify_last_object_removed
     *     Called after the last object has been removed from the cache.
     */
    void set_callbacks(const std::function<void()> &notify_first_object_inserted,
                       const std::function<void()> &notify_garbage_collection_needed,
                       const std::function<void(ID::List)> &notify_object_removed,
                       const std::function<void()> &notify_last_object_removed)
    {
        notify_first_object_inserted_ = notify_first_object_inserted;
        notify_garbage_collection_needed_ = notify_garbage_collection_needed;
        notify_object_removed_ = notify_object_removed;
        notify_last_object_removed_ = notify_last_object_removed;
    }

    static constexpr const ssize_t USED_ENTRY_ALREADY_UP_TO_DATE = -1;
    static constexpr const ssize_t USED_ENTRY_INVALID_ID         = -2;

    /*!
     * Update object's time stamp and parents' time stamps to the current time.
     *
     * \returns
     *   The depth of the \p entry, or #USED_ENTRY_ALREADY_UP_TO_DATE in case
     *   the entry was already up to date. Note that a return value of
     *   #USED_ENTRY_ALREADY_UP_TO_DATE does \e not indicate an error condition
     *   (this function cannot fail).
     *
     * \see
     *     #LRU::Cache::use(ID::List id)
     */
    ssize_t use(const std::shared_ptr<Entry> entry);

    /*!
     * Update time stamp of object with given ID and its parents' time stamps.
     *
     * \returns
     *   The depth of the entry with ID \p id, #USED_ENTRY_ALREADY_UP_TO_DATE
     *   in case the entry was already up to date, or #USED_ENTRY_INVALID_ID in
     *   case the given list ID was the invalid ID.
     *
     * \see
     *     #LRU::Cache::use(const std::shared_ptr<Entry>)
     */
    ssize_t use(ID::List id);

    /*!
     * Get ID of pinned object, if any.
     */
    ID::List get_pinned_object() const { return pinned_object_id_; }

    /*!
     * Pin object in cache, never allow it or object on the path to the root to
     * be garbage collected.
     *
     * \param id
     *     ID of the object to keep in cache. Pass the invalid ID to remove the
     *     pinned path and allow it to be garbage collected.
     *
     * returns
     *     True in case some object is (possibly still) pinned after this
     *     function returns, false in case no object is pinned upon return.
     */
    bool pin(ID::List id);

  private:
    static ssize_t unlink_objects_on_path_to_root(Entry *entry,
                                                  const Entry *&oldest,
                                                  const Entry *&second_youngest);
    static void link_objects_on_path_to_root(Entry *entry,
                                             const Timebase::time_point &now);
    static bool pin_or_unpin_objects_on_path_to_root(const Cache &cache,
                                                     ID::List id,
                                                     bool pin_them);

  public:
    /*!
     * Insert entry into cache, assign ID.
     *
     * \param entry
     *     The object to be inserted. The LRU::CacheMetaData::id_ of its
     *     #LRU::Entry::cache_data_ field will be set to some none-zero value
     *     by this function. It is an error to attempt to insert an object that
     *     has been inserted into some cache before.
     *
     * \param cmode
     *     Whether or not the object shall remain in cache on garbage
     *     collection. Uncached objects always get garbage collected if they
     *     are not pinned, cached objects remain in cache until they expire.
     *
     * \param ctx
     *    Context of the entry. Pass 0 if there is only one context.
     *
     * \param size_of_entry
     *     The (possibly estimated) size of the new entry. This value should
     *     comprise of the whole space occupied by the object and all data
     *     embedded inside. It is usually not enough to pass
     *     \c sizeof(#LRU::Entry) here because the cached objects are very
     *     likely to contain some more data.
     *
     * \returns
     *     A list identifier for the inserted entry, or an invalid identifier
     *     on failure.
     *
     * \see
     *     #ID::List::is_valid()
     */
    ID::List insert(std::shared_ptr<Entry> &&entry, CacheMode cmode,
                    // cppcheck-suppress passedByValue
                    const ID::List::context_t ctx,
                    size_t size_of_entry);

    /*!
     * Re-insert entry into cache to assign a new ID.
     *
     * \param entry
     *     Object to be re-inserted.
     */
    ID::List insert_again(std::shared_ptr<Entry> &&entry);

    /*!
     * Look up cached object by ID.
     *
     * This function does not update the object's time stamp nor that of its
     * parents. Use #LRU::Cache::use() for this purpose.
     *
     * \param entry_id
     *     The ID of the cached entry (list) to be looked up.
     *
     * \returns
     *     A smart pointer referencing the entry corresponding to the ID. The
     *     smart pointer may be empty, in which case there is no object in the
     *     cache corresponding to the given ID.
     */
    std::shared_ptr<Entry> lookup(ID::List entry_id) const;

    /*!
     * Change the size of a cached object, automatically mark as used.
     *
     * In case a cached object's size has changed as a result of some object
     * mutation, the new size should be passed to the cache. The cache will use
     * this value to revaluate memory consumption and to decide whether or not
     * it needs to discard any entries from the cache to stay within the
     * configured limits.
     *
     * \param entry_id
     *     The ID of the cached entry (list) whose size has changed.
     *
     * \param size_of_entry
     *     The new size of the object.
     *
     * \returns
     *     True on success, false if there is no object in the cache
     *     corresponding to the given ID.
     */
    bool set_object_size(ID::List entry_id, size_t size_of_entry);

  private:
    /*!
     * Discard given object in the cache.
     *
     * \param candidate
     *     A pointer to the oldest unpinned cache object.
     *
     * \param allow_notifications
     *     If \c false, then the callbacks for object removal set by
     *     #LRU::Cache::set_callbacks() are \e not called by this function.
     *     (see #LRU::Cache::notify_object_removed_ and
     *     #LRU::Cache::notify_last_object_removed_).
     *
     * \returns
     *     A pointer to the next object that is younger than \p candidate, or
     *     \c nullptr in case there is no such object.
     */
    const Entry *discard(const Entry *const candidate,
                         bool allow_notifications = true);

  public:
    /*!
     * Run garbage collection on the cache.
     *
     * In case any configured limits are exceeded, this function discards the
     * least recently used objects from the cache until the size of the cache
     * does not exceed any of the configured limits anymore.
     *
     * This function needs to be called periodically, there is no thread in the
     * background that does this automatically. It should be called from some
     * context in which it is safe to discard objects from the cache.
     *
     * \returns
     *     The amount of time after which this function should be called again.
     *     The duration is at most #LRU::Cache::maximum_age_threshold_ if there
     *     are objects stored in the cache, or std::chrono::seconds::max() if
     *     the cache is empty.
     */
    std::chrono::seconds gc();

    /*!
     * Get Number of objects in the cache.
     */
    size_t count() const;

    /*!
     * Perform in-place topological sort of the given list of IDs.
     *
     * The given list must fulfill the following properties:
     *
     * - The list must contain at least one leaf.
     * - Each internal node in the list must be reachable from some leaf in the
     *   list using parent links, either directly or through other nodes in the
     *   list.
     * - IDs may occur only once.
     *
     * \returns
     *     True on success, false in case the topological sort failed. In case
     *     of failure, the list content may be corrupted after this function
     *     returns and should not be used anymore.
     */
    bool toposort_for_purge(const std::vector<ID::List>::iterator &kill_list_begin,
                            const std::vector<ID::List>::iterator &kill_list_end) const;

    /*!
     * Remove all entries in given list of IDs.
     *
     * The list \e must contain all entries of one or more full subtrees,
     * including all leaf and internal nodes; otherwise, the cache structure
     * will become inconsistent and likely cause a program crash at some point.
     *
     * Also, the list \e must enumerate lists IDs in topological order, leaves
     * first. If you cannot make sure that your list is properly sorted, then
     * #LRU::Cache::toposort_for_purge() should be called before calling this
     * function.
     */
    void purge_entries(const std::vector<ID::List>::const_iterator &kill_list_begin,
                       const std::vector<ID::List>::const_iterator &kill_list_end,
                       bool allow_notifications = true);

    class const_iterator
    {
      public:
        explicit const_iterator(const Entry *e): current_(e) {}

        bool operator!=(const const_iterator &other) const
        {
            return current_ != other.current_;
        }

        const_iterator &operator++()
        {
            if(current_ != nullptr)
                current_ = Entry::AgingList::next_younger(*current_);

            return *this;
        }

        const Entry &operator*() const
        {
            return *current_;
        }


      private:
        const Entry *current_;
    };

    /*!
     * Const interator over aging list.
     */
    const const_iterator begin() const
    {
        return const_iterator(oldest_object_);
    }

    /*!
     * End of aging list.
     */
    static const const_iterator end()
    {
        return const_iterator(nullptr);
    }


    class const_reverse_iterator
    {
      public:
        explicit const_reverse_iterator(const Entry *e): current_(e) {}

        bool operator!=(const const_reverse_iterator &other) const
        {
            return current_ != other.current_;
        }

        const_reverse_iterator &operator++()
        {
            if(current_ != nullptr)
                current_ = Entry::AgingList::next_older(*current_);

            return *this;
        }

        const Entry &operator*() const
        {
            return *current_;
        }


      private:
        const Entry *current_;
    };

    /*!
     * Const reverse interator over aging list.
     */
    const const_reverse_iterator rbegin() const
    {
        return const_reverse_iterator(root_object_);
    }

    /*!
     * End of reverse aging list.
     */
    static const const_reverse_iterator rend()
    {
        return const_reverse_iterator(nullptr);
    }

    void dump_pointers(std::ostream &os, const char *detail = nullptr) const;
    void self_check() const;
};

};

/*!@}*/

#endif /* !LRU_HH */
