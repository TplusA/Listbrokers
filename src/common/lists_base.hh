/*
 * Copyright (C) 2015, 2016, 2018, 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef LISTS_BASE_HH
#define LISTS_BASE_HH

#include "lru.hh"
#include "logged_lock.hh"
#include "messages.h"
#include "de_tahifi_lists_errors.hh"
#include "de_tahifi_lists_item_kinds.hh"

#include <string>
#include <array>
#include <vector>
#include <limits>
#include <utility>
#include <thread>
#include <deque>
#include <atomic>
#include <algorithm>

template <typename T, uint16_t> class TiledList;

class ListIterException: public std::runtime_error
{
  private:
    ListError error_;

  public:
    explicit ListIterException(const std::string &what_arg, const ListError::Code error):
        std::runtime_error(what_arg),
        error_(error)
    {}

    explicit ListIterException(const std::string &what_arg, const ListError error):
        std::runtime_error(what_arg),
        error_(error)
    {}

    explicit ListIterException(const char *what_arg, const ListError::Code error):
        std::runtime_error(what_arg),
        error_(error)
    {}

    explicit ListIterException(const char *what_arg, const ListError error):
        std::runtime_error(what_arg),
        error_(error)
    {}

    virtual ~ListIterException() {}

    ListError get_list_error() const
    {
        return error_;
    }
};

/*!
 * Class template for one item in a list.
 *
 * Each #ListItem_ structure represents one piece of actual data stored in some
 * list. Inside, a structure of list-specific data is stored, enabling any kind
 * of data being managed in some list that uses #ListItem_ structures as basic
 * unit of storage.
 *
 * A set of fundamental functions is defined for the #ListItem_ type, more
 * functions may be defined for the embedded list-specific type (obtained via
 * #ListItem_::get_specific_data() functions).
 *
 * \tparam T
 *     Data specific to the stored item. Note that we are using some kind of
 *     static duck-typing instead of dynamic virtual functions here. We expect
 *     \p T to define certain functions, and if they are missing, the compiler
 *     throws an error. The approach used here avoids storing a vtable pointer
 *     for each item (there can be tens or hundreds of thousands of items).
 */
template <typename T>
class ListItem_
{
  public:
    ListItem_(const ListItem_ &) = delete;
    ListItem_ &operator=(const ListItem_ &) = delete;
    ListItem_(ListItem_ &&) = default;
    ListItem_ &operator=(ListItem_ &&) = default;

    explicit ListItem_() {}

  private:
    ID::List child_;
    T data_;

  public:
    /*!
     * Reset structure to intialized state, should behave roughly like a ctor.
     */
    void reset()
    {
        data_.reset();
        child_ = ID::List();
    }

    /*!
     * Get human-readable name for presentation.
     */
    void get_name(std::string &name) const
    {
        data_.get_name(name);
    }

    /*!
     * The kind of this item.
     */
    ListItemKind get_kind() const
    {
        return data_.get_kind();
    };

    void set_child_list(ID::List child)
    {
        log_assert(child.is_valid());
        log_assert(!child_.is_valid());
        child_ = child;
    }

    void obliviate_child()
    {
        log_assert(child_.is_valid());
        child_ = ID::List();
    }

    /*!
     * Get cache identifier of the child list.
     *
     * \returns
     *     If not a directory or if the child list does not exist yet, then the
     *     invalid ID is returned; otherwise valid ID of a cached child list is
     *     returned.
     */
    ID::List get_child_list() const
    {
        return child_;
    }

    /*! Return const stored data for type-specific code. */
    const T &get_specific_data() const
    {
        return data_;
    }

    /*! Return stored data for type-specific code. */
    T &get_specific_data()
    {
        return data_;
    }
};

template<typename T, uint16_t tile_size>
class ListThreads;

enum class ListTileState
{
    FREE,
    FILLING,
    READY,
    CANCELED,
    ERROR,
};

/*!
 * Class template for a single tile in a tiled list.
 *
 * Tiles are filled in by worker threads and must be locked before reading.
 * There are some fundamental assumptions about all code that accesses list
 * tiles:
 * - All read accesses are done within a single thread.
 * - All write accesses are done within other threads.
 * - Each tile is filled by a single thread, but multiple tiles may be filled
 *   at the same time by different threads, one thread per tile.
 * - The same thread that does the read accesses schedules the write accesses,
 *   i.e., it controls the point in time at which it becomes unsafe to read
 *   from a list tile without acquiring a lock.
 * - Each writing thread locks the list tile it is working on and releases the
 *   lock when it is done.
 * - When a writer releases the lock, it either means that the tile has been
 *   filled or that an error has occurred. Therefore, the reading thread must
 *   check the tile's error state right after lock acquisition.
 *
 * \note
 *     The software design does not consider reading from multiple threads
 *     (something that might sound useful, but isn't at the moment) nor
 *     multiple writers (something that is completely irrelevant at the
 *     moment). Trying to do this without precautions is bound to fail.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 *
 * \tparam tile_size
 *     How many items to store per tile. See also #TiledList.
 */
template <typename T, uint16_t tile_size>
class ListTile_
{
  private:
    LoggedLock::Mutex write_lock_;
    LoggedLock::ConditionVariable tile_processed_;
    std::atomic<bool> cancel_filling_request_;

    std::array<ListItem_<T>, tile_size> items_;

    uint32_t base_;
    uint16_t stored_items_count_;
    ListTileState state_;
    ListError error_;

  public:
    ListTile_(const ListTile_ &) = delete;
    ListTile_ &operator=(const ListTile_ &) = delete;

    explicit ListTile_():
        base_(0),
        stored_items_count_(0),
        state_(ListTileState::FREE),
        error_(ListError::INTERNAL)
    {
        static_assert(tile_size > 0, "Tile size must be positive");
        LoggedLock::configure(write_lock_, "ListTile_::write_lock_",
                              MESSAGE_LEVEL_DEBUG);
        LoggedLock::configure(tile_processed_, "ListTile_::tile_processed_-cv",
                              MESSAGE_LEVEL_DEBUG);
    }

    ~ListTile_()
    {
        /* in case a thread is still referencing this tile, we need to wait for
         * it to finish */
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(write_lock_);
    }

    LoggedLock::UniqueLock<LoggedLock::Mutex> lock_tile()
    {
        return LoggedLock::UniqueLock<LoggedLock::Mutex>(write_lock_);
    }

    LoggedLock::UniqueLock<LoggedLock::Mutex> try_lock_tile()
    {
        return LoggedLock::UniqueLock<LoggedLock::Mutex>(write_lock_, std::try_to_lock);
    }

    bool is_tile_for(ID::Item idx) const
    {
        return idx.get_raw_id() >= base_ && idx.get_raw_id() < uint32_t(base_ + tile_size);
    }

    /*!
     * Clear tile, optionally set specific error state.
     *
     * \remark
     *     This function may only be called while holding the tile lock or when
     *     it is known that no worker thread is accessing the tile.
     */
    void reset(ListError error = ListError(ListError::OK),
               ListTileState state = ListTileState::FREE)
    {
        for(size_t i = 0; i < tile_size; ++i)
            items_[i].reset();

        base_ = 0;
        stored_items_count_ = 0;
        error_ = error;
        state_ = state;
    }

    /*!
     * Check whether or not this tile is free.
     *
     * \remark
     *     This function is thread-safe if called from the reading thread.
     *     Writers should not call this function.
     */
    bool is_free() const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        auto lock(const_cast<ListTile_ *>(this)->try_lock_tile());

        return lock.owns_lock() && state_ == ListTileState::FREE;
    }

    /*!
     * Get tile state.
     *
     * \remark
     *     This function may only be called while holding the tile lock.
     */
    ListTileState get_state() const
    {
        return state_;
    }

    /*!
     * Callback from filling thread: Error or canceled.
     *
     * This function is used to set the tile to an "incomplete" state. Filling
     * has finished, but the data that was supposed to be filled in is not
     * available.
     *
     * \param error
     *     The reason why the tile has not been filled in is passed here. In
     *     case the \p error is #ListError::OK, the tile is set to
     *     #ListTileState::CANCELED state, meaning a successful cancelation of
     *     filling the tile with content on purpose. Otherwise, the state is
     *     set to ListTileState::ERROR and the error code is stored in the tile
     *     for evaluation at some later point when attempting to read from the
     *     tile.
     *
     * \remark
     *     This function is called with the tile lock held by the calling
     *     thread.
     */
    void canceled_notification(ListError error)
    {
        reset(error,
              (error == ListError::OK) ? ListTileState::CANCELED : ListTileState::ERROR);

        tile_processed_.notify_all();
    }

    /*!
     * Callback from filling thread: Tile is ready for use.
     *
     * \remark
     *     This function is called with the tile lock held by the calling
     *     thread.
     */
    void done_notification(uint16_t count)
    {
        stored_items_count_ += count;
        log_assert(stored_items_count_ <= tile_size);
        state_ = ListTileState::READY;

        tile_processed_.notify_all();
    }

    /*!
     * Mark tile as occupied for filling.
     *
     * \pre Tile is free and neither in error state nor occupied.
     *
     * \remark
     *     This function must be called from the reading thread.
     */
    ListTile_ *activate_tile(ID::Item idx)
    {
        log_assert(idx.get_raw_id() <= std::numeric_limits<decltype(base_)>::max());
        log_assert(state_ == ListTileState::FREE);

        base_ = idx.get_raw_id();
        base_ -= idx.get_raw_id() % tile_size;
        state_ = ListTileState::FILLING;

        cancel_filling_request_ = false;

        return this;
    }

    void cancel()
    {
        cancel_filling_request_ = true;
    }

    bool is_requesting_cancel() const
    {
        return cancel_filling_request_;
    }

  protected:
    /*!
     * Lock tile, wait for ready state if not ready yet.
     *
     * \exception #ListIterException
     *     This function throws a #ListIterException in case the tile is not
     *     ready after acquiring the lock.
     */
    void wait_for_ready_state(const char *const exception_text)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        auto lock(lock_tile());

        tile_processed_.wait(lock,
            [this]() { return state_ != ListTileState::FILLING; });

        if(state_ != ListTileState::READY)
            throw ListIterException(exception_text, error_);
    }

  public:
    /*!
     * Get number of items stored in this tile.
     *
     * This function blocks until the tile has been filled by some thread.
     *
     * \exception #ListIterException
     *     This function throws a #ListIterException in case the tile is not
     *     ready after acquiring the lock.
     *
     * \remark
     *     This function is thread-safe if called from the reading thread.
     *     Writers should not call this function.
     */
    uint16_t size() const
    {
        const_cast<ListTile_ *>(this)->wait_for_ready_state("Cannot get size of tile");
        return stored_items_count_;
    }

    uint32_t get_base() const
    {
        return base_;
    }

    /*!
     * Get list item stored in this tile.
     *
     * This function blocks until the tile has been filled by some thread.
     *
     * \param raw_index
     *     Return a const reference to the item at this index. Must be
     *     non-negative and smaller than \p tile_size.
     *
     * \exception #ListIterException
     *     This function throws a #ListIterException in case the tile is not
     *     ready after acquiring the lock.
     *
     * \remark
     *     This function is thread-safe if called from the reading thread.
     *     Writers should not call this function.
     */
    const ListItem_<T> &get_list_item_by_raw_index(uint16_t raw_index) const
    {
        const_cast<ListTile_ *>(this)->wait_for_ready_state("Cannot get item from tile");
        return items_[raw_index];
    }

  private:
    /*!
     * Return pointer to raw internal #ListItem_ array.
     *
     * \remark
     *     This function is called from a filling thread while holding the tile
     *     lock. It does so to initialize an #ItemProvider object to get at the
     *     list items and write to them. The reader thread should not call this
     *     function.
     */
    ListItem_<T> *get_items_data()
    {
        return items_.data();
    }

  public:
    class ItemProviderExtra
    {
        static ListItem_<T> *get_items_data(ListTile_ &tile)
        {
            return tile.get_items_data();
        }

        friend class ListThreads<T, tile_size>;
    };
};

/*!
 * Simplistic forward iterator over list items for filling them with data.
 *
 * The #TiledListFillerIface interface passes an #ItemProvider object to its
 * #TiledListFillerIface::fill() function. The corresponding implementation
 * takes the next list item it should fill by calling the #ItemProvider::next()
 * function.
 *
 * The main purpose of the #ItemProvider class is to enable decoupling the item
 * fill-in function implementation from the concrete list type whose items
 * shall be filled.
 */
template <typename T>
class ItemProvider
{
  private:
    ListItem_<T> *const items_;
    const size_t items_count_;
    size_t next_item_index_;

  public:
    ItemProvider(const ItemProvider &) = delete;
    ItemProvider &operator=(const ItemProvider &) = delete;

    constexpr explicit ItemProvider(ListItem_<T> *const items, size_t count):
        items_(items),
        items_count_(count),
        next_item_index_(0)
    {}

    T *next()
    {
        return (next_item_index_ < items_count_
                ? &items_[next_item_index_++].get_specific_data()
                : static_cast<T *>(nullptr));
    }
};

/*!
 * Interface for filling in list items on demand.
 *
 * The #ListTiles_ tile manangement object uses objects implementing this
 * interface to fill the tiles when their contents are needed.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 *
 * \attention
 *     Objects that implement this interface are used from multiple threads.
 *     All interface implementations must be thread-safe.
 */
template <typename T>
class TiledListFillerIface
{
  protected:
    explicit TiledListFillerIface() {}

  public:
    TiledListFillerIface(TiledListFillerIface &&) = default;
    TiledListFillerIface &operator=(TiledListFillerIface &&) = default;
    TiledListFillerIface(const TiledListFillerIface &) = default;
    TiledListFillerIface &operator=(const TiledListFillerIface &) = delete;

    virtual ~TiledListFillerIface() {}

    /*!
     * \param item_provider
     *     Interface to the tile items to be filled. It is guaranteed that for
     *     any tile no more than one thread is calling this function for that
     *     tile referenced through an #ItemProvider object. That is, an
     *     implementation of this interface must be prepared to be executed in
     *     several parallel instances, but it may assume that at any time all
     *     instances are going to work on disjoint tiles.
     *
     * \param list_id
     *     The list ID the tile refers to, i.e., which cached object the tile
     *     belongs to.
     *
     * \param idx
     *     Index of the item in the whole list that should be placed at the
     *     first position in the tile to be filled.
     *
     * \param count
     *     Number of items that fit into the tile, i.e., the tile size.
     *
     * \param[out] error
     *     The kind of error that occurred while attempting to fill the tile,
     *     if any. In case no error occurred, this parameter is set to
     *     #ListError::OK.
     *
     * \param may_continue
     *     Function that should be called at convenient points to check whether
     *     or not filling should be continued. This function will return
     *     \c false in case the user interrupts downloading of list contents
     *     (e.g., by quickly jumping through the list or by leaving a slowly
     *     loading list).
     *
     * \returns
     *     Number of items written to the tile, or -1 in case of hard error.
     *     Note that the \p error parameter may still indicate an error even if
     *     the return value is non-negative.
     */
    virtual ssize_t fill(ItemProvider<T> &item_provider, ID::List list_id,
                         ID::Item idx, size_t count, ListError &error,
                         const std::function<bool()> &may_continue) const = 0;
};

/*!
 * Classs template for managing threads that fill list tiles.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 *
 * \tparam tile_size
 *     How many items to store per tile. See also #TiledList.
 */
template <typename T, uint16_t tile_size>
class ListThreads
{
  private:
    std::vector<std::thread> threads_;

    class Work
    {
      public:
        ListTile_<T, tile_size> *tile_;
        const TiledListFillerIface<T> *filler_;
        ID::List list_id_;

        Work(const Work &) = default;
        Work &operator=(const Work &) = delete;
        Work(Work &&) = default;
        Work &operator=(Work &&) = default;

        explicit Work(ListTile_<T, tile_size> &tile,
                      const TiledListFillerIface<T> &filler,
                      ID::List list_id):
            tile_(&tile),
            filler_(&filler),
            list_id_(list_id)
        {}
    };

    struct WorkQueue
    {
        LoggedLock::Mutex lock_;
        LoggedLock::ConditionVariable work_available_;
        std::deque<Work> work_;
        std::atomic<bool> shutdown_request_;

        constexpr explicit WorkQueue():
            shutdown_request_(false)
        {
            LoggedLock::configure(lock_, "ListThreads::WorkQueue::lock_",
                                  MESSAGE_LEVEL_DEBUG);
            LoggedLock::configure(work_available_,
                                  "ListThreads::WorkQueue::work_available_-cv",
                                  MESSAGE_LEVEL_DEBUG);
        }
    };

    WorkQueue work_queue_;

    bool is_synchronous_mode_;

  public:
    ListThreads(const ListThreads &) = delete;
    ListThreads &operator=(const ListThreads &) = delete;

    constexpr explicit ListThreads(bool synchronized):
        is_synchronous_mode_(synchronized)
    {}

    ~ListThreads()
    {
        shutdown();
    }

    void set_synchronized() { is_synchronous_mode_ = true; }

    /*!
     * Start thread pool with given number of threads.
     *
     * For best performance, the number of threads should match or exceed the
     * number of tiles in a tiled list.
     *
     * In unit tests and for debugging, it is often convenient to start the
     * thread pool with only a single thread. This way, items in the work queue
     * are being worked on in a serial fashion in the order they were inserted,
     * but still in parallel to the main thread.
     */
    void start(size_t number_of_threads)
    {
        log_assert(threads_.empty());
        log_assert(work_queue_.work_.empty());
        log_assert(number_of_threads > 0);

        work_queue_.shutdown_request_ = false;

        for(size_t i = 0; i < number_of_threads; ++i)
            threads_.emplace_back(std::thread(worker, &work_queue_));
    }

    /*!
     * Wait until the work queue becomes empty.
     *
     * Note that the worker threads are not necessarily idle when this function
     * returns. All threads may still be busy processing their current work
     * item. The queue, however, will be empty when this function returns.
     */
    void wait_empty()
    {
        while(1)
        {
            {
                LOGGED_LOCK_CONTEXT_HINT;
                std::lock_guard<LoggedLock::Mutex> qlock(work_queue_.lock_);

                if(work_queue_.work_.empty())
                    return;
            }

            std::this_thread::yield();
        }
    }

    void wait_empty_if_synchronized()
    {
        if(is_synchronous_mode_)
            wait_empty();
    }

    /*!
     * Stop thread pool.
     *
     * \returns
     *     The number of threads that were running in the thread pool.
     */
    size_t shutdown()
    {
        if(work_queue_.shutdown_request_.exchange(true))
            return 0;

        if(threads_.empty())
            return 0;

        {
            LOGGED_LOCK_CONTEXT_HINT;
            std::lock_guard<LoggedLock::Mutex> lock(work_queue_.lock_);
            work_queue_.work_available_.notify_all();
        }

        for(auto &t : threads_)
            t.join();

        auto count = threads_.size();

        threads_.clear();

        return count;
    }

    void enqueue(ListTile_<T, tile_size> &tile,
                 const TiledListFillerIface<T> &filler, ID::List list_id)
    {
        log_assert(!threads_.empty());
        log_assert(tile.get_state() == ListTileState::FILLING);

        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(work_queue_.lock_);
        work_queue_.work_.emplace_back(tile, filler, list_id);
        work_queue_.work_available_.notify_one();
    }

    /*!
     * Clear fill queue.
     *
     * This function does not affect tiles that are already processed by some
     * thread. Use #ListThreads::cancel_filler() for this purpose.
     */
    void cancel_all_queued_fillers()
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::Mutex> qlock(work_queue_.lock_);

        for(auto &work : work_queue_.work_)
        {
            log_assert(work.tile_->get_state() == ListTileState::FILLING);
            work.tile_->canceled_notification(ListError());
            log_assert(work.tile_->get_state() == ListTileState::CANCELED);
        }

        work_queue_.work_.clear();
    }

    /*!
     * Cancel filling given tile.
     *
     * Cancelation works as follows.
     * -# Put the tile into a request-cancel mode by calling
     *    #ListTile_::cancel(). That function sets a flag that the worker
     *    threads are checking by calling #ListTile_::is_requesting_cancel()
     *    from time to time. If canceling is requested, then the worker thread
     *    will stop filling the tile it is currently working on. It will also
     *    notify the main thread about cancelation of \p tile.
     * -# Lock the work queue so that worker threads cannot dequeue more work.
     *    A thread that just dequeued work before the lock has been acquired
     *    will see the cancel request and cancel by itself.
     * -# Try to lock the given \p tile.
     *    -# If locking \p tile succeeds, then either (1) the tile is still in
     *       the queue; (2) the tile has been processed already and is either
     *       ready for use or in an error state; or (3) the tile has been
     *       processed by a thread, but processing has been canceled already.
     *       In case of (1), in which case the tile state is
     *       #ListTileState::FILLING, search for the tile in the locked queue
     *       and remove it from the queue. In case of (1) or (2), in which
     *       case the tile state is not #ListTileState::CANCELED, notify the
     *       main thread about cancelation of \p tile.
     *    -# If locking \p tile fails, then a thread is currently processing
     *       the tile and it will react to the cancel state. In this case,
     *       unlock the work queue to keep other threads going, then lock \p
     *       tile. Locking is likely going to block, but only until the worker
     *       sees the cancel state (or finishes the tile some other way).
     *
     * \param tile
     *     The tile that should not be filled anymore. It is an error to call
     *     this function for free tiles.
     *
     * \see #ListTile_::canceled_notification()
     */
    void cancel_filler(ListTile_<T, tile_size> &tile)
    {
        /* if a thread is already working on this tile, then let it know */
        tile.cancel();

        /* in the meantime, check if the tile is still in the queue */
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::Mutex> qlock(work_queue_.lock_);
        auto tlock(tile.try_lock_tile());

        if(tlock.owns_lock())
        {
            const auto tstate = tile.get_state();

            /* got the lock, so the tile is not being processed */
            if(tstate == ListTileState::FILLING)
            {
                /* should be in queue */
                for(auto it = work_queue_.work_.begin(); it != work_queue_.work_.end(); ++it)
                {
                    if(it->tile_ == &tile)
                    {
                        work_queue_.work_.erase(it);
                        break;
                    }
                }
            }

            if(tstate != ListTileState::CANCELED)
                tile.canceled_notification(ListError());
        }
        else
        {
            /* there must be thread working on this tile, so wait for it stop
             * doing it; no need to hold the queue lock anymore */
            qlock.unlock();

            LOGGED_LOCK_CONTEXT_HINT;
            auto temp(tile.lock_tile());
        }

        /* The tile state may now be anything except for #ListTileState::FREE
         * and #ListTileState::FILLING. The processing thread may have finished
         * just before we have taken the lock, it may have encountered an
         * error, or it may have just seen our cancel request.
         *
         * A state of #ListTileState::FREE state would indicate a programming
         * error because tiles in the queue are never supposed to be in that
         * state. A state of #ListTileState::FILLING would also be a
         * programming error because this state indicates that the tile is
         * about to be or currently being processed. */
        log_assert(tile.get_state() != ListTileState::FREE);
        log_assert(tile.get_state() != ListTileState::FILLING);
    }

  private:
    /*!
     * Thread: Attempt to fill a tile using given filler.
     */
    static void worker(WorkQueue *queue)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::Mutex> qlock(queue->lock_, std::defer_lock);

        while(1)
        {
            LOGGED_LOCK_CONTEXT_HINT;
            qlock.lock();

            queue->work_available_.wait(qlock,
                [&queue]()
                {
                    return queue->shutdown_request_ || !queue->work_.empty();
                });

            if(queue->shutdown_request_)
                break;

            /* copy work data to our own stack, lock the tile, unlock the
             * queue, fill the tile --- IN THIS ORDER! */
            const Work work_item(queue->work_.front());
            queue->work_.pop_front();

            LOGGED_LOCK_CONTEXT_HINT;
            auto tlock(work_item.tile_->lock_tile());

            qlock.unlock();

            do_fill_tile(work_item);
        }
    }

    /*!
     * Helper for #ListThreads::worker() for readability.
     *
     * \pre The tile to be filled is locked by us.
     */
    static void do_fill_tile(const Work &work_item)
    {
        ItemProvider<T>
            item_provider(ListTile_<T, tile_size>::ItemProviderExtra::get_items_data(*work_item.tile_),
                          tile_size);

        ListError error;
        const ssize_t count =
            work_item.filler_->fill(item_provider, work_item.list_id_,
                                    ID::Item(work_item.tile_->get_base()),
                                    tile_size, error,
                                    [&work_item]()
                                    {
                                        return !work_item.tile_->is_requesting_cancel();
                                    } );

        if(count > 0)
            work_item.tile_->done_notification(count);
        else if(count < 0)
        {
            msg_error(0, LOG_ERR,
                      "Failed filling tile from list %u, index %u",
                      work_item.list_id_.get_raw_id(),
                      work_item.tile_->get_base());
            work_item.tile_->canceled_notification(error);
        }
    }
};

/*!
 * Class template for managing lists in tiles instead of flat lists.
 *
 * This class is concerned with the management of center, up, and down tiles.
 *
 * \tparam T
 *     Domain-specific data to be stored per list item.
 *
 * \tparam tile_size
 *     How many items to store per tile. See also #TiledList.
 */
template <typename T, uint16_t tile_size>
class ListTiles_
{
  public:
    /*!
     * Number of tiles maintained by the tile cache.
     */
    static constexpr size_t maximum_number_of_active_tiles = 3;

    /*!
     * Number of items that can be stored in cache in best case.
     */
    static constexpr size_t maximum_number_of_hot_items =
        maximum_number_of_active_tiles * tile_size;

    enum class ItemLocation
    {
        NIL    = -1,
        UP     = 0,
        CENTER = 1,
        DOWN   = 2,
    };

  private:
    ListThreads<T, tile_size> &thread_pool_;

    std::array<ListTile_<T, tile_size>, maximum_number_of_active_tiles> hot_tiles_;
    std::array<ListTile_<T, tile_size> *, maximum_number_of_active_tiles> active_tiles_;

    ssize_t get_backing_store_index_for_tile(ItemLocation which)
    {
        if(active_tiles_[size_t(which)] == nullptr)
            return -1;
        else
            return active_tiles_[size_t(which)] - hot_tiles_.begin();
    }

    /*!
     * Find a free tile in the tile cache.
     *
     * \returns
     *     The index of a free tile in #ListTiles_::hot_tiles_, or
     *     #ListTiles_::maximum_number_of_active_tiles in case there is no free
     *     tile.
     *
     * \remark
     *     This function is thread-safe if called from the reading thread.
     *     Writers should not call this function.
     */
    size_t find_free_tile() const
    {
        return std::distance(
                    hot_tiles_.begin(),
                    std::find_if(hot_tiles_.begin(), hot_tiles_.end(),
                                 [] (const auto &t) { return t.is_free(); }));
    }

    class SyncThreads
    {
      private:
        ListThreads<T, tile_size> &thread_pool_;

      public:
        SyncThreads(const SyncThreads &) = delete;
        SyncThreads &operator=(const SyncThreads &) = delete;

        constexpr explicit SyncThreads(ListThreads<T, tile_size> &pool):
            thread_pool_(pool)
        {}

        ~SyncThreads() { thread_pool_.wait_empty_if_synchronized(); }
    };

  public:
    ListTiles_(const ListTiles_ &) = delete;
    ListTiles_ &operator=(const ListTiles_ &) = delete;

    /*!
     * Construct a #ListTiles_ object.
     *
     * \param threads
     *     Filler thread pool whose threads fetch data from the network and
     *     place them into list tiles.
     */
    explicit ListTiles_(ListThreads<T, tile_size> &threads):
        thread_pool_(threads),
        active_tiles_({nullptr, nullptr, nullptr})
    {
        static_assert(tile_size > 0, "Tile size must be positive");
    }

    ~ListTiles_()
    {
        clear();
    }

  private:
    /*!
     * Check which tile contains the given index, if any.
     *
     * \returns
     *     Either #ListTiles_::ItemLocation::UP,
     *     #ListTiles_::ItemLocation::CENTER, or
     *     #ListTiles_::ItemLocation::DOWN in case the index is in some cached
     *     tile, or #ListTiles_::ItemLocation::NIL in case the index in not in
     *     cache.
     */
    ItemLocation contains(ID::Item idx) const
    {
        for(auto i = 0U; i < active_tiles_.size(); ++i)
        {
            if(active_tiles_[i] != nullptr && active_tiles_[i]->is_tile_for(idx))
                return ItemLocation(i);
        }

        return ItemLocation::NIL;
    }

    /*!
     * Compute some (any) index in a tile that is adjacent to the tile the item
     * with the given index lies in.
     *
     * This function does not access any list elements, it only computes in the
     * domain of list indices.
     *
     * \param idx
     *     Index from which an origin tile is computed from.
     *
     * \param total_number_of_items
     *     Total number of items in the list.
     *
     * \param direction
     *     Which adjacent tile to consider.
     *
     * \returns
     *     Some valid index that lies within the tile adjacent to the origin
     *     tile, or \p total_number_of_items of items in case \p direction is
     *     #ListTiles_::ItemLocation::NIL.
     */
    static ID::Item index_in_adjacent_tile(ID::Item idx, size_t total_number_of_items,
                                           ItemLocation direction)
    {
        ID::Item item(total_number_of_items);

        switch(direction)
        {
          case ItemLocation::NIL:
            break;

          case ItemLocation::UP:
            if(idx.get_raw_id() >= tile_size)
                item = ID::Item(idx.get_raw_id() - tile_size);
            else
                item = ID::Item(total_number_of_items - 1);
            break;

          case ItemLocation::CENTER:
            item = idx;
            break;

          case ItemLocation::DOWN:
            if(idx.get_raw_id() + tile_size < total_number_of_items)
                item = ID::Item(idx.get_raw_id() + tile_size);
            else
                item = ID::Item(0);
            break;
        }

        return item;
    }

    /*!
     * Low level tile cache sliding logic.
     *
     * \param filler
     *     Object that knows how to fill our list items.
     *
     * \param list_id
     *     Which list to fill.
     *
     * \param idx
     *     Some index in the new center tile
     *
     * \param total_number_of_items
     *     Total size of the list in terms of number of items.
     *
     * \param tile_to_keep, tile_to_push_out
     *     Which tile is going to drop out and which is going to be kept when
     *     sliding the tile cache. One of them must be
     *     #ListTiles_::ItemLocation::UP, the other must be
     *     #ListTiles_::ItemLocation::DOWN. Together, they define the sliding
     *     direction.
     */
    void slide(const TiledListFillerIface<T> &filler,
               ID::List list_id, ID::Item idx, size_t total_number_of_items,
               ItemLocation tile_to_push_out, ItemLocation tile_to_keep)
    {
        ListTile_<T, tile_size> *const temp = active_tiles_[size_t(tile_to_push_out)];;
        active_tiles_[size_t(tile_to_push_out)] = active_tiles_[size_t(ItemLocation::CENTER)];
        active_tiles_[size_t(ItemLocation::CENTER)] = active_tiles_[size_t(tile_to_keep)];
        active_tiles_[size_t(tile_to_keep)] = temp;

        log_assert(active_tiles_[size_t(tile_to_push_out)] != nullptr);

        const ID::Item adjacent_index =
            index_in_adjacent_tile(idx, total_number_of_items, tile_to_keep);

        if(temp != nullptr)
        {
           if(!temp->is_tile_for(adjacent_index))
           {
               log_assert(!temp->is_free());
               thread_pool_.cancel_filler(*temp);

               /* no locking of temp tile required here because the only thread
                * that has been working on this tile, if any, was told to stop
                * doing it in #ListThreads::cancel_filler(), and that function
                * also waited for the thread to release the lock on temp */
               temp->reset();
           }
           else
           {
               /* the list is short and resides completely in memory */
               log_assert(active_tiles_[size_t(ItemLocation::CENTER)] != nullptr);
               return;
           }
        }

        const SyncThreads sync_filler(thread_pool_);

        if(active_tiles_[size_t(ItemLocation::CENTER)] == nullptr)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "materialize center tile around index %u", idx.get_raw_id());

            size_t tile_index = find_free_tile();
            log_assert(tile_index < maximum_number_of_active_tiles);
            auto tile = hot_tiles_[tile_index].activate_tile(idx);
            active_tiles_[size_t(ItemLocation::CENTER)] = tile;

            thread_pool_.enqueue(*tile, filler, list_id);
        }

        if(temp != nullptr)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "materialize adjacent tile around index %u", adjacent_index.get_raw_id());
            auto tile = temp->activate_tile(adjacent_index);
            thread_pool_.enqueue(*tile, filler, list_id);
        }
    }

    void slide_up(const TiledListFillerIface<T> &filler,
                  ID::List list_id, ID::Item idx,
                  size_t total_number_of_items, const unsigned int steps)
    {
        log_assert(steps > 0);
        log_assert(steps < maximum_number_of_hot_items);

        for(unsigned int i = 0; i < steps; ++i)
            slide(filler, list_id,
                  ID::Item(idx.get_raw_id() + (steps - i - 1) * tile_size),
                  total_number_of_items,
                  ItemLocation::DOWN, ItemLocation::UP);
    }

    void slide_down(const TiledListFillerIface<T> &filler,
                    ID::List list_id, ID::Item idx,
                    size_t total_number_of_items, const unsigned int steps)
    {
        log_assert(steps > 0);
        log_assert(steps < maximum_number_of_hot_items);

        for(unsigned int i = 0; i < steps; ++i)
            slide(filler, list_id,
                  ID::Item(idx.get_raw_id() - (steps - i - 1) * tile_size),
                  total_number_of_items,
                  ItemLocation::UP, ItemLocation::DOWN);
    }

    /*!
     * Attempt to fill all tiles using given filler.
     *
     * \exception #ListIterException
     *     This function throws a #ListIterException in case the filler fails
     *     hard to fill any tile.
     */
    void fill(const TiledListFillerIface<T> &filler, ID::List list_id,
              ID::Item center_idx, size_t total_number_of_items)
    {
        clear();

        if(total_number_of_items == 0)
            return;

        const SyncThreads sync_filler(thread_pool_);

        /* center tile first */
        auto tile = hot_tiles_[0].activate_tile(center_idx);
        active_tiles_[size_t(ItemLocation::CENTER)] = tile;

        thread_pool_.enqueue(*tile, filler, list_id);

        if(total_number_of_items <= tile_size)
            return;

        /* have enough items to fill down-tile */
        const ID::Item down_idx((tile->get_base() < total_number_of_items - tile_size)
                                ? tile->get_base() + tile_size
                                : 0);

        tile = hot_tiles_[1].activate_tile(down_idx);
        active_tiles_[size_t(ItemLocation::DOWN)] = tile;

        thread_pool_.enqueue(*tile, filler, list_id);

        if(total_number_of_items <= 2U * tile_size)
            return;

        /* have enough items to fill up-tile as well */
        tile = active_tiles_[size_t(ItemLocation::CENTER)];
        const ID::Item up_idx((tile->get_base() > 0)
                              ? tile->get_base() - tile_size
                              : total_number_of_items - 1);

        tile = hot_tiles_[2].activate_tile(up_idx);
        active_tiles_[size_t(ItemLocation::UP)] = tile;

        thread_pool_.enqueue(*tile, filler, list_id);
    }

  public:
    bool empty() const
    {
        return std::all_of(active_tiles_.begin(), active_tiles_.end(),
                           [] (const auto &t) { return t == nullptr; });
    }

    bool prefetch(const TiledListFillerIface<T> &filler, ID::List list_id,
                  ID::Item first, size_t count, size_t total_number_of_items,
                  bool auto_slide)
    {
        if(count == 0)
            return false;

        const uint16_t position_of_first_in_tile = first.get_raw_id() % tile_size;

        if(count + position_of_first_in_tile > maximum_number_of_hot_items)
        {
            /* need more than three tiles for this */
            return false;
        }

        unsigned int required_number_of_slides;
        unsigned int number_of_spanned_tiles;
        const ItemLocation slide_direction =
            check_overlapping_range_for_prefetch(first, count, required_number_of_slides,
                                                 number_of_spanned_tiles);

        if(required_number_of_slides == 0)
        {
           if(auto_slide && (slide_direction == ItemLocation::UP ||
                             slide_direction == ItemLocation::DOWN))
               required_number_of_slides = 1;
           else
           {
               /* sliding up or down not required, and automatic sliding is either
                * not requested or not applicable */
               msg_vinfo(MESSAGE_LEVEL_DEBUG,
                         "no need to prefetch index %u, already in cache",
                        first.get_raw_id());
               return true;
           }
        }

        ID::Item center_index;

        if((auto_slide && number_of_spanned_tiles < maximum_number_of_active_tiles) ||
           (slide_direction == ItemLocation::NIL && number_of_spanned_tiles < maximum_number_of_active_tiles) ||
           (slide_direction == ItemLocation::DOWN && required_number_of_slides == 1) ||
           required_number_of_slides == 0)
        {
            /* fewer than three tiles required, up tile remains empty */
            center_index = first;
        }
        else
        {
            /* need three tiles, make sure first ends up in the up tile */
            center_index = ID::Item(first.get_raw_id() + tile_size);
        }

        switch(slide_direction)
        {
          case ItemLocation::CENTER:
            BUG("Invalid slide direction");
            break;

          case ItemLocation::UP:
            if(required_number_of_slides == 0)
                msg_vinfo(MESSAGE_LEVEL_DEBUG,
                          "no need to prefetch index %u, already in cache",
                         first.get_raw_id());
            else
            {
                msg_vinfo(MESSAGE_LEVEL_DEBUG,
                          "slide up to index %u", first.get_raw_id());
                slide_up(filler, list_id, center_index, total_number_of_items,
                         required_number_of_slides);
            }

            return true;

          case ItemLocation::DOWN:
            if(required_number_of_slides == 0)
                msg_vinfo(MESSAGE_LEVEL_DEBUG,
                          "no need to prefetch index %u, already in cache",
                         first.get_raw_id());
            else
            {
                msg_vinfo(MESSAGE_LEVEL_DEBUG,
                          "slide down to index %u", first.get_raw_id());
                slide_down(filler, list_id, center_index, total_number_of_items,
                           required_number_of_slides);
            }

            return true;

          case ItemLocation::NIL:
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "prefetch %zu items, starting at index %u", count, first.get_raw_id());
            fill(filler, list_id, center_index, total_number_of_items);
            return true;
        }

        throw ListIterException("Invalid slide direction", ListError::INTERNAL);

        return false;
    }

  private:
    static unsigned int
    compute_number_of_required_slides(ItemLocation &direction, bool is_first_item,
                                      unsigned int number_of_spanned_tiles)
    {
        switch(direction)
        {
          case ItemLocation::UP:
            if(is_first_item)
                return 0;
            else
                return number_of_spanned_tiles - 1;

          case ItemLocation::DOWN:
            if(is_first_item)
                return number_of_spanned_tiles - 1;
            else
                return 0;

          case ItemLocation::CENTER:
            {
                unsigned int ret = number_of_spanned_tiles >= 2 ? number_of_spanned_tiles - 2 : 0;

                if(ret > 0)
                    direction = is_first_item ? ItemLocation::UP : ItemLocation::DOWN;

                return ret;
            }

          case ItemLocation::NIL:
            break;
        }

        return maximum_number_of_active_tiles;
    }

    ItemLocation check_overlapping_range_for_prefetch(ID::Item first,
                                                      size_t count,
                                                      unsigned int &required_number_of_slides,
                                                      unsigned int &number_of_spanned_tiles) const
    {
        const uint16_t position_of_first_in_tile =
            first.get_raw_id() % tile_size;

        number_of_spanned_tiles =
            1 + (position_of_first_in_tile + count - 1) / tile_size;

        log_assert(number_of_spanned_tiles >= 1);
        log_assert(number_of_spanned_tiles <= maximum_number_of_active_tiles);

        ItemLocation retval = contains(first);

        if(retval != ItemLocation::NIL)
        {
            required_number_of_slides =
                compute_number_of_required_slides(retval, true, number_of_spanned_tiles);

            log_assert(required_number_of_slides < maximum_number_of_active_tiles);

            return retval;
        }

        if(number_of_spanned_tiles > 1)
        {
            ID::Item last(first.get_raw_id() + count - 1);

            retval = contains(last);

            if(retval != ItemLocation::NIL)
            {
                required_number_of_slides =
                    compute_number_of_required_slides(retval, false, number_of_spanned_tiles);

                log_assert(required_number_of_slides < maximum_number_of_active_tiles);

                return retval;
            }
        }

        required_number_of_slides = maximum_number_of_active_tiles;

        return ItemLocation::NIL;
    }

    void clear()
    {
        thread_pool_.cancel_all_queued_fillers();

        for(auto &ht : hot_tiles_)
        {
            if(!ht.is_free())
            {
                thread_pool_.cancel_filler(ht);
                ht.reset();
            }
        }

        active_tiles_.fill(nullptr);
    }

  public:
    /*!
     * Iterator for stored #ListItem_ structures.
     */
    class const_iterator
    {
      private:
        const ListTiles_ &src_;
        const ItemLocation last_tile_;
        ItemLocation which_tile_;
        uint16_t idx_;
        ListError first_list_error_;

        static constexpr ItemLocation determine_last_tile(ItemLocation first)
        {
            return (first == ItemLocation::DOWN
                    ? ItemLocation::CENTER
                    : (first == ItemLocation::UP
                       ? ItemLocation::DOWN
                       : (first == ItemLocation::CENTER
                          ? ItemLocation::UP
                          : ItemLocation::NIL)));
        }

      public:
        explicit const_iterator(const ListTiles_ &src, uint16_t idx, ItemLocation which_tile):
            src_(src),
            last_tile_(determine_last_tile(which_tile)),
            which_tile_(which_tile),
            idx_(0),
            first_list_error_(ListError::OK)
        {
            find_first();

            if(which_tile_ != ItemLocation::NIL)
                idx_ = idx;
        }

        constexpr explicit const_iterator(const ListTiles_ &src):
            src_(src),
            last_tile_(ItemLocation::NIL),
            which_tile_(ItemLocation::NIL),
            idx_(0),
            first_list_error_(ListError::OK)
        {}

        bool operator!=(const const_iterator &other) const
        {
            return which_tile_ != other.which_tile_ || idx_ != other.idx_;
        }

        const_iterator &operator++()
        {
            if(which_tile_ == ItemLocation::NIL)
                throw ListIterException("Cannot step beyond end of ListTiles_::const_iterator",
                                        get_list_error_code());

            if(step())
                find_first();

            return *this;
        }

        const ListItem_<T> &operator*() const
        {
            if(which_tile_ == ItemLocation::NIL)
                throw ListIterException("Cannot dereference end of ListTiles_::const_iterator",
                                        get_list_error_code());

            return src_.active_tiles_[size_t(which_tile_)]->get_list_item_by_raw_index(idx_);
        }

        const ListItem_<T> *operator->() const
        {
            return &(this->operator*());
        }

        uint32_t get_item_id() const
        {
            return src_.active_tiles_[size_t(which_tile_)]->get_base() + idx_;
        }

      private:
        void put_list_error(const ListError &e)
        {
            if(first_list_error_ == ListError::OK)
                first_list_error_ = e;
        }

        ListError::Code get_list_error_code() const
        {
            if(first_list_error_ != ListError::OK)
                return first_list_error_.get();
            else
                return ListError::INTERNAL;
        }

        bool next_tile()
        {
            idx_ = 0;

            if(which_tile_ == ItemLocation::NIL || which_tile_ == last_tile_)
                which_tile_ = ItemLocation::NIL;

            switch(which_tile_)
            {
              case ItemLocation::NIL:
                break;

              case ItemLocation::DOWN:
                which_tile_ = ItemLocation::UP;
                return true;

              case ItemLocation::UP:
                which_tile_ = ItemLocation::CENTER;
                return true;

              case ItemLocation::CENTER:
                which_tile_ = ItemLocation::DOWN;
                return true;
            }

            return false;
        }

        bool step()
        {
            try
            {
                if(++idx_ < src_.active_tiles_[size_t(which_tile_)]->size())
                    return true;
                else
                    return next_tile();
            }
            catch(const ListIterException &e)
            {
                put_list_error(e.get_list_error());
                return next_tile();
            }
        }

        void find_first()
        {
            while(which_tile_ != ItemLocation::NIL)
            {
                if(src_.active_tiles_[size_t(which_tile_)] == nullptr)
                    (void)next_tile();
                else
                {
                    try
                    {
                        if(idx_ < src_.active_tiles_[size_t(which_tile_)]->size())
                            break;
                        else
                            (void)step();
                    }
                    catch(const ListIterException &e)
                    {
                        put_list_error(e.get_list_error());
                        (void)next_tile();
                    }
                }
            }
        }
    };

    const_iterator begin(ID::Item first) const
    {
        return const_iterator(*this,
                              first.get_raw_id() % tile_size,
                              contains(first));
    }

    const_iterator begin() const
    {
        return const_iterator(*this, 0, ItemLocation::UP);
    }

    constexpr const_iterator end() const
    {
        return const_iterator(*this);
    }

    ListItem_<T> &get_list_item_unsafe(ID::Item id)
    {
        return const_cast<ListItem_<T> &>(static_cast<const ListTiles_ *>(this)->get_list_item_unsafe(id));
    }

    const ListItem_<T> &get_list_item_unsafe(ID::Item id) const
    {
        const auto center_tile = active_tiles_[size_t(ItemLocation::CENTER)];
        return center_tile->get_list_item_by_raw_index(id.get_raw_id() - center_tile->get_base());
    }

    class ClearTile
    {
        static inline void clear(ListTiles_ &tiles)
        {
            tiles.clear();
        }

        /* for resetting a list to empty state without having to delete it*/
        friend class TiledList<T, tile_size>;
    };
};

#endif /* !LISTS_BASE_HH */
