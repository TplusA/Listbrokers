/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef LISTS_HH
#define LISTS_HH

#include <functional>
#include <memory>

#include "lists_base.hh"
#include "lru.hh"

namespace Airable { class RemoteList; }

/*!
 * Generic interface for accessing lists.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 */
template <typename T>
class GenericList
{
  public:
    using ListItemType = ListItem_<T>;

  protected:
    explicit GenericList() {}

  public:
    GenericList(const GenericList &) = delete;
    GenericList &operator=(const GenericList &) = delete;

    virtual ~GenericList() {}

    /*!
     * Number of items supposed to be found in the list.
     *
     * Note that for certain list types it is possible for any of these items
     * not to be in RAM. The size returned by this function is the logical
     * number of items in the list, not the number of items currently
     * physically stored.
     */
    virtual size_t size() const = 0;

    /*!
     * Find child item by cache ID.
     *
     * This function operates on physically stored items. That is, it will
     * return nothing for an item that should be there, but isn't (yet, or
     * anymore).
     *
     * \note
     *     This function is probably not very interesting for high-level code.
     *     Code that actually wants to see data should call the
     *     \c enter_child() function implemented by the various list types
     *     (e.g., #UPnP::ServerList::enter_child()).
     */
    virtual const ListItemType *lookup_child_by_id(ID::List child_id) const = 0;

    /*!
     * Find item ID of link to child item by child's cache ID.
     *
     * This function operates on physically stored items. That is, it will
     * return nothing for an item that should be there, but isn't (yet, or
     * anymore).
     *
     * \param child_id
     *     Cache ID of the list supposedly directly referenced by this list.
     *
     * \param[out] idx
     *     Item ID of the item that links to list \p child_id, if any. Remains
     *     untouched in case the sought list is not a direct child of this
     *     list.
     *
     * \returns
     *     True if the list is a direct child of this list, false otherwise.
     *
     * \see #GenericList::lookup_child_by_id()
     */
    virtual bool lookup_item_id_by_child_id(ID::List child_id, ID::Item &idx) const = 0;

    /*!
     * Direct access to children by item index.
     *
     * Works on logical items and materializes them if necessary. Note that
     * out-of-bounds accesses are not caught, just like for other containers.
     */
    virtual ListItemType &operator[](ID::Item idx) = 0;

    /*
     * Direct read-only access to children by item index.
     *
     * Works on logical items and materializes them if necessary. Note that
     * out-of-bounds accesses are not caught, just like for other containers.
     */
    virtual const ListItemType &operator[](ID::Item idx) const = 0;
};


/*!
 * Class template for flat lists.
 *
 * A flat list is basically a wrapper around a \c std::vector storing
 * #ListItem_ structures. Such lists are useful for small data that is
 * reasonable to keep in memory all the time.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 */
template <typename T>
class FlatList: public LRU::Entry, public GenericList<T>
{
  private:
    std::vector<ListItem_<T>> items_;

  public:
    FlatList(const FlatList &) = delete;
    FlatList &operator=(const FlatList &) = delete;

    explicit FlatList(std::shared_ptr<Entry> parent):
        LRU::Entry(parent)
    {}

    virtual ~FlatList() {}

    /*!
     * Iterate over all #ListItem_ structures stored in the list (non-\c const
     * version).
     */
    typename std::vector<ListItem_<T>>::iterator begin()
    {
        return items_.begin();
    }

    typename std::vector<ListItem_<T>>::iterator end()
    {
        return items_.end();
    }

    /*!
     * Iterate over all #ListItem_ structures stored in the list (\c const
     * version).
     */
    typename std::vector<ListItem_<T>>::const_iterator begin() const
    {
        return items_.begin();
    }

    typename std::vector<ListItem_<T>>::const_iterator end() const
    {
        return items_.end();
    }

    /*!
     * Number of items stored in the list.
     */
    typename std::vector<ListItem_<T>>::size_type size() const override
    {
        return items_.size();
    }

    /*!
     * Append item to list, keeping it unsorted.
     */
    void append_unsorted(ListItem_<T> &&item)
    {
        items_.emplace_back(std::move(item));
    }

    void insert_before(size_t idx, ListItem_<T> &&item)
    {
        items_.insert(items_.begin() + idx, std::move(item));
    }

    ID::List FIXME_remove(ID::Item idx)
    {
        log_assert(idx.get_raw_id() < items_.size());
        ID::List id = items_[idx.get_raw_id()].get_child_list();
        items_.erase(items_.begin() + idx.get_raw_id());
        return id;
    }

    const ListItem_<T> *lookup_child_by_id(ID::List child_id) const override
    {
        const auto &found(std::find_if(items_.begin(), items_.end(),
                                [child_id] (const auto &it)
                                { return it.get_child_list() == child_id; }));
        return found != items_.end() ? &*found : nullptr;
    }

    bool lookup_item_id_by_child_id(ID::List child_id, ID::Item &idx) const override
    {
        for(auto it = items_.begin(); it != items_.end(); ++it)
        {
            if(it->get_child_list() == child_id)
            {
                idx = ID::Item(it - items_.begin());
                return true;
            }
        }

        return false;
    }

    const ListItem_<T> &operator[](ID::Item idx) const override
    {
        return items_[idx.get_raw_id()];
    }

    ListItem_<T> &operator[](ID::Item idx) override
    {
        return items_[idx.get_raw_id()];
    }
};

/*!
 * Class template for lists managed in tiles.
 *
 * For very large data or in case retrieval of data is a time-consuming effort,
 * it is much better to fetch data only when needed than attempting to fetch
 * and store all data that is available.
 *
 * The #TiledList does this by using three consecutive chunks of data---the
 * <em>tiles</em>---which represent only a small fraction of a potentially
 * large list. There is a \e center \e tile which stores the partial list
 * containing the most recently accessed item; it is filled when it is
 * accessed. In addition, there are two adjacent tiles called the \e up \e tile
 * and the \e down \e tile. These are used to prefetch the content that comes
 * before and after the center tile, respectively, and are dropped in as new
 * center tile when necessary. This way, scrolling up or down a list does not
 * lead to perceivable delays.
 *
 * Random access to elements is supported, but the array subscript operator
 * should not be used to enumerate a range of items (i.e., for showing them on
 * a display). The subscript operator is implemented so that it attempts to
 * keep the tiles centered around the accessed item. It may juggle the tiles
 * around---and fill some or all of them---on each access, leading to
 * unnecessary item content requests. Instead, the function template
 * #for_each_item() should be used for iterating over ranges.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 *
 * \tparam tile_size
 *     How many items to store per tile. This is the number of items retrieved
 *     in block when a specific item inside that block is asked for (comparable
 *     to harddisk sectors or flash read blocks).
 *
 * \attention
 *     Objects implementing this interface are used from multiple threads while
 *     their internal tiles are filled in parallel. All interfaces
 *     implementation must be thread-safe.
 */
template <typename T, uint16_t tile_size>
class TiledList: public LRU::Entry, public GenericList<T>
{
  private:
    size_t number_of_entries_;
    ListTiles_<T, tile_size> tiles_;
    static ListThreads<T, tile_size> thread_pool;

  protected:
    const TiledListFillerIface<T> &filler_;

  public:
    TiledList(const TiledList &) = delete;
    TiledList &operator=(const TiledList &) = delete;

    explicit TiledList(std::shared_ptr<Entry> parent,
                       size_t number_of_entries,
                       const TiledListFillerIface<T> &filler):
        LRU::Entry(parent),
        number_of_entries_(number_of_entries),
        tiles_(thread_pool),
        filler_(filler)
    {
        static_assert(tile_size > 0, "Tile size must be positive");
    }

    virtual ~TiledList() {}

    /*!
     * Start networking threads for this type of list.
     */
    static void start_threads(unsigned int number_of_threads,
                              bool synchronous_mode)
    {
        if(synchronous_mode)
            thread_pool.set_synchronized();

        thread_pool.start(number_of_threads);
    }

    /*!
     * Stop networking threads for this type of list.
     */
    static void shutdown_threads() { thread_pool.shutdown(); }

    /*!
     * Wait until all queued work has been processed.
     *
     * Note that the worker threads are not necessarily idle when this function
     * returns. All threads may still be busy processing their current work
     * item. The queue, however, will be empty when this function returns.
     */
    static void sync_threads()
    {
        thread_pool.wait_empty();
        thread_pool.start(thread_pool.shutdown());
    }

    /*!
     * Fill cache with elements from specified range.
     *
     * \returns
     *     True in case the elements were loaded into or already present in the
     *     cache, false in case the cache was not changed and the elements are
     *     not in cache. The latter case would happen if the requested elements
     *     would not fit into cache (either because too many items were
     *     requested or because they span too many tiles), or if \p count is 0.
     *
     * \exception #ListIterException
     *     Thrown in case prefetching fails hard.
     */
    bool prefetch_range(ID::Item first, size_t count) const
    {
        return
            const_cast<TiledList *>(this)->tiles_.prefetch(filler_,
                                                           get_cache_id(),
                                                           first, count,
                                                           number_of_entries_,
                                                           false);
    }

  private:
    /*!
     * Explicit request to load tiles surrounding the given item.
     *
     * If the item is not present in any of the tiles, then all tiles are
     * erased and loaded according to the item.
     *
     * If the item is present in any tile, then the tile cache centers around
     * the item's tile. Any tile that drops out on one side of the current
     * window is reused on the other side.
     *
     * \returns
     *     True if the materialization was successful, false otherwise.
     */
    bool materialize(ID::Item idx, ListError &error)
    {
        if(!idx.is_valid())
        {
            error = ListError::INVALID_ID;
            return false;
        }

        if(idx.get_raw_id() >= number_of_entries_)
        {
            BUG("requested tile list materialization around %u, but have only %zu items",
                idx.get_raw_id(), number_of_entries_);
            error = ListError::INTERNAL;
            return false;
        }

        return tiles_.prefetch(filler_, get_cache_id(), idx, 1,
                               number_of_entries_, true);
    }

  public:
    using const_iterator = typename decltype(tiles_)::const_iterator;

    /*!
     * Iterate over all #ListItem_ structures stored in the list (\c const
     * version).
     */
    const_iterator begin(ID::Item first = ID::Item(0)) const
    {
        return tiles_.begin(first);
    }

    constexpr const_iterator end() const
    {
        return tiles_.end();
    }

    size_t size() const override
    {
        return number_of_entries_;
    }

  private:
    void deferred_set_size(size_t new_size)
    {
        log_assert(number_of_entries_ == 0);
        log_assert(tiles_.empty());
        number_of_entries_ = new_size;
    }

    void clear_all()
    {
        decltype(tiles_)::ClearTile::clear(tiles_);
        number_of_entries_ = 0;
    }

  public:
    const ListItem_<T> *lookup_child_by_id(ID::List child_id) const override
    {
        for(const auto &it : tiles_)
        {
            /* our custom iterator doesn't work with std::find_if() atm */
            // cppcheck-suppress useStlAlgorithm
            if(it.get_child_list() == child_id)
                return &it;
        }

        return nullptr;
    }

    bool lookup_item_id_by_child_id(ID::List child_id, ID::Item &idx) const override
    {
        for(auto it = tiles_.begin(); it != tiles_.end(); ++it)
        {
            if(it->get_child_list() == child_id)
            {
                idx = ID::Item(it.get_item_id());
                return true;
            }
        }

        return false;
    }

    /*!
     * \copydoc #TiledList::operator[](ID::Item) const
     */
    ListItem_<T> &operator[](ID::Item idx) override
    {
        return const_cast<ListItem_<T> &>(static_cast<const TiledList *>(this)->operator[](idx));
    }

    /*!
     * Random access to elements in the cache.
     *
     * Note that this operator attempts to keep the indexed element in the
     * center tile. This means that the up and down tiles are updated as soon
     * as the index is not in the center tile. It might be possible to slide up
     * or down so that two tiles (center and either up or down) are kept, but
     * the up or down tile will become the new center tile; the adjacent tile
     * is automatically going to be filled with data.
     *
     * This means that it is \e not suited for repetitive sweeps over ranges in
     * the list that span tile boundaries. It will work, but unnecessary cache
     * thrashing will take place.
     *
     * \see
     *     #for_each_item(), #TiledList::prefetch_range()
     */
    const ListItem_<T> &operator[](ID::Item idx) const override
    {
        ListError error;

        if(const_cast<TiledList *>(this)->materialize(idx, error))
            return tiles_.get_list_item_unsafe(idx);
        else
            throw ListIterException("Tile materialization failed", error);
    }

    class ManipulateRootList
    {
        static inline void deferred_set_size(TiledList<T, tile_size> &list,
                                             size_t new_size)
        {
            list.deferred_set_size(new_size);
        }

        static inline void clear_all(TiledList<T, tile_size> &list)
        {
            list.clear_all();
        }

        /* for manipulating the top-level list, (re-)fetched from server */
        friend ::Airable::RemoteList;
    };
};

template <typename ListType>
static ID::List
add_child_list_to_cache(LRU::Cache &cache, ID::List parent_id,
                        LRU::CacheMode cmode, const ID::List::context_t ctx,
                        size_t estimated_size_in_ram)
{
    auto list = std::make_shared<ListType>(cache.lookup(parent_id));
    if(list == nullptr)
        return ID::List();

    return cache.insert(list, cmode, ctx, estimated_size_in_ram);
}

template <typename ListType, typename FillerType>
static ID::List
add_child_list_to_cache(LRU::Cache &cache, ID::List parent_id,
                        LRU::CacheMode cmode, const ID::List::context_t ctx,
                        size_t number_of_items, size_t estimated_size_in_ram,
                        const TiledListFillerIface<FillerType> &filler)
{
    auto list = std::make_shared<ListType>(cache.lookup(parent_id),
                                           number_of_items, filler);
    if(list == nullptr)
        return ID::List();

    return cache.insert(list, cmode, ctx, estimated_size_in_ram);
}

/*!
 * How the generic #for_each_item() implementation should work.
 */
template <typename T>
struct ForEachItemTraits;

/*!
 * Efficient iteration over range of items in a list.
 *
 * For each item in the range with (up to) \p count items starting at \p first
 * in list \p list, the function \p apply is called. The function must return
 * true to continue iteration, false to stop (this is not necessarily an error
 * condition). Use lambda expressions and lambda captures for passing extra
 * data to this function.
 *
 * This function template tries to operate on cached values only. For a
 * #TiledList it attempts to keep the tiles the way they are. In case the whole
 * requested range is in the tile cache, no external fetches are performed. For
 * a #FlatList, nothing special is done. The compiler should be able to
 * optimize the code to a bare \c for loop in this case.
 */
template <typename T, typename ItemType, typename ListTypeTraits = ForEachItemTraits<T>>
static ListError for_each_item(std::shared_ptr<T> list,
                               ID::Item first, size_t count,
                               const std::function<bool(ID::Item, ItemType)> &apply)
{
    if(list == nullptr)
        return ListError(ListError::INVALID_ID);

    const size_t end =
        (count > 0)
        ? std::min(first.get_raw_id() + count, list->size())
        : list->size();

    if(first.get_raw_id() >= end)
    {
        if(count > 0)
            msg_error(0, LOG_WARNING,
                      "WARNING: Client requested %zu items starting at index %u, "
                      "but list size is %zu",
                      count, first.get_raw_id(), end);

        return ListError();
    }

    ListError error;

    try
    {
        if(ListTypeTraits::warm_up_cache(list, first, ID::Item(end)))
        {
            /* can use cache-friendly version */
            typename ListTypeTraits::IterType iter = ListTypeTraits::begin(list, first);

            for(size_t i = first.get_raw_id(); i < end; ++i)
            {
                if(!apply(ID::Item(i), ListTypeTraits::get_next_cached_element(list, iter)))
                    break;
            }
        }
        else
        {
            /* too many elements or no caching mechanism, use simple iteration */
            for(size_t i = first.get_raw_id(); i < end; ++i)
            {
                if(!apply(ID::Item(i), (*list)[ID::Item(i)]))
                    break;
            }
        }
    }
    catch(ListIterException &e)
    {
        msg_error(EFAULT, LOG_ERR,
                  "Failed iterating over list range [%zu, %zu): %s",
                  size_t(first.get_raw_id()), end, e.what());
        error = e.get_list_error();
    }

    return error;
}

/*!
 * Traits to be used by \p for_each_item() for a generic #TiledList.
 *
 * Specializations of #TiledList likely will have to provide a similar traits
 * structure. They may call this generic implementation to fill in their
 * implementation (compiler optimizations will shrink these calls down to
 * almost nothing).
 */
template <typename ItemType, uint16_t tile_size>
struct ForEachItemTraits<const TiledList<ItemType, tile_size>>
{
    using ListType = const TiledList<ItemType, tile_size>;
    using IterType = typename ListType::const_iterator;

    static bool warm_up_cache(std::shared_ptr<ListType> list,
                              ID::Item first, ID::Item end)
    {
        const size_t count = end.get_raw_id() - first.get_raw_id();

        return list->prefetch_range(first, count);
    }

    static IterType begin(std::shared_ptr<ListType> list, ID::Item first)
    {
        return list->begin(first);
    }

    static const ListItem_<ItemType> &
    get_next_cached_element(std::shared_ptr<ListType> list, IterType &iter)
    {
        const auto &ret = *iter;
        ++iter;
        return ret;
    }
};

/*!
 * Traits to be used by \p for_each_item() for a generic #FlatList.
 *
 * Specializations of #FlatList likely will have to provide a similar traits
 * structure. They may call this generic implementation to fill in their
 * implementation (compiler optimizations will shrink these calls down to
 * almost nothing).
 */
template <typename ItemType>
struct ForEachItemTraits<const FlatList<ItemType>>
{
    using ListType = const FlatList<ItemType>;
    using IterType = size_t;

    static constexpr bool warm_up_cache(std::shared_ptr<ListType> list,
                                        ID::Item first, ID::Item end)
    {
        return false;
    }

    static IterType begin(std::shared_ptr<ListType> list, ID::Item first)
    {
        BUG("%s(): unexpected call", __PRETTY_FUNCTION__);
        return first.get_raw_id();
    }

    static const ListItem_<ItemType> &
    get_next_cached_element(std::shared_ptr<ListType> list, IterType &iter)
    {
        BUG("%s(): unexpected call", __PRETTY_FUNCTION__);
        return (*list)[ID::Item(iter++)];
    }
};

#endif /* !LISTS_HH */
