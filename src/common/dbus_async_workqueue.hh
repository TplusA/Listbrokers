/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_ASYNC_WORKQUEUE_HH
#define DBUS_ASYNC_WORKQUEUE_HH

#include "dbus_async_work.hh"

#include <condition_variable>
#include <thread>
#include <list>

namespace DBusAsync
{

/*!
 * Representation of a D-Bus work queue, possibly with a worker thread.
 *
 * Multiple work queue instances may be created to offload different kinds of
 * work to different queues, and thus threads. In an extreme scenario, it would
 * be possible to create one queue per D-Bus request, each with its own thread,
 * and destroy it when the work is done. It is, however, more practical to keep
 * at most one work queue per D-Bus method around (per method, not for each
 * invocation) and reuse them for subsequent calls. Multiple kinds of work may
 * be processed in one work queue (one after the other), and each kind of work
 * may be processed by multiple work queue (for improved throughput).
 *
 * The internal maximum queue length can be configured. In case the queue
 * length is exceeded, the currently running work is canceled.
 */
class WorkQueue
{
  public:
    enum class Mode
    {
        SYNCHRONOUS,
        ASYNC,
    };

  private:
    std::mutex lock_;
    const Mode mode_;
    const size_t maximum_queue_length_;

    std::shared_ptr<Work> work_in_progress_;
    std::condition_variable work_finished_;
    std::list<std::shared_ptr<Work>> queue_;
    bool is_accepting_work_;

    std::thread thread_;

  public:
    WorkQueue(const WorkQueue &) = delete;
    WorkQueue &operator=(const WorkQueue &) = delete;

    explicit WorkQueue(Mode mode, size_t maximum_queue_length = 0):
        mode_(mode),
        maximum_queue_length_(maximum_queue_length),
        is_accepting_work_(true),
        thread_(mode == Mode::ASYNC ? std::thread(worker, this) : std::thread())
    {}

    /*!
     * Stop processing work, do not accept further work items.
     *
     * This function tries to stop the worker thread, if any, gracefully. It
     * will, however, block for as long as the currently processed work takes
     * to complete.
     *
     * Only the current work item is processed, all other work items will be
     * removed from the queue. The queue will be empty when this function
     * returns.
     */
    void shutdown();

    /*!
     * Add work to be processed.
     *
     * If the work queue has been initialized to employ a worker thread
     * (asynchronous mode), then the work item is added to the queue and will
     * be processed by the worker thread. In this case, this function will
     * return more or less instantly.
     *
     * If no worker thread has been enabled (synchronous mode), then the work
     * item is added to the queue. In case the newly added work is the only
     * item in the queue, it is immediately executed in the context of the
     * caller. This function will return only when the work is done. In case
     * the newly added work is not the first item in the queue (i.e., another
     * thread as added some other work and is currently working on it), then
     * this function will block until (1) all other work items in the queue
     * have been processed by the other threads that have added the work, and
     * (2) the added work has been processed by the caller.
     *
     * In any case, the currently executed work is canceled (if possible) if
     * the maximum queue length is exceeded. The \p work is appended to the end
     * of the queue shifting older queued work items out of the queue.
     *
     * In synchronous mode, the queue serializes work by blocking threads. Be
     * aware that this may cause deadlocks. Setting the queue length to 1 is a
     * way to make sure that only one instance of a certain kind of work is
     * being processed, with new work replacing work in progress (by
     * canceling).
     *
     * \param work
     *     Do this.
     *
     * \param work_accepted
     *     Function which supports implementing sync/async-agnostic client
     *     code. When the work has been added to the queue (so it *will* be
     *     processed), but before the work has had a chance of being processed,
     *     this function is called. Its first parameter tells if the work about
     *     to be processed asynchronously (\c true) or in context of the caller
     *     (\c false). The second parameter is used in case of synchronous
     *     execution, in which case the function is called twice; once before
     *     the work is processed (second parameter is \c false), once after
     *     (second parameter is \c true). The purpose of this function is to be
     *     able to report D-Bus handler completion when the work is accepted,
     *     but before it is finished. This avoids a race condition with
     *     \c de.tahifi.Lists.Navigation.DataAvailable and
     *     \c de.tahifi.Lists.Navigation.DataError.
     *
     * \returns
     *     True if the work item was added to the queue for asynchronous
     *     processing, false if the item has been processed synchronously or
     *     the queue does not accept any more work (system is shutting down).
     */
    bool add_work(std::shared_ptr<Work> &&work,
                  std::function<void(bool, bool)> &&work_accepted);

  private:
    /*!
     * Put work into queue for asynchronous processing.
     *
     * In case the new work item cannot be added to the queue because it is
     * full, the work in progress is cancelled and replaced by the next item
     * from the queue, if any, or with the new work item in case the maximum
     * queue size is 0.
     *
     * In any case, this function guarantees that
     * #DBusAsync::WorkQueue::work_in_progress_ points to a valid work item
     * when the function returns.
     *
     * Must be called while holding #DBusAsync::WorkQueue::lock_.
     *
     * \returns
     *     True if the added work is first in queue, false if the work has been
     *     added to the end of the queue.
     */
    bool queue_work(std::shared_ptr<Work> work);

    /*!
     * Wait for work to arive, and process it.
     *
     * This function blocks until either new work is available or until the
     * queue is shut down.
     *
     * In case new work becomes available, it is processed if it is runnable.
     * When done, that work item is removed and the next item from the queue,
     * if any, is becomes the next new work item. This work, however, is
     * processed only the next time this function is called. That is, this
     * function must be called in a loop to keep the queue going.
     *
     * In case the queue is shut down, the work in progress and all queued work
     * items, if any, are cancelled. The function returns \c false in this
     * case.
     *
     * \param lock
     *     Must wrap #DBusAsync::WorkQueue::lock_ and must be locked.
     *
     * \param work
     *     A specific work item to process. Pass \c nullptr for asynchronous
     *     mode (processing done by worker thread on a first come, first serve
     *     basis). Pass pointer to a work item in synchronous mode (processing
     *     done by calling thread).
     *
     * \returns
     *     True if the work item has been processed, false if the queue is
     *     shutting down.
     */
    bool process_work_item(std::unique_lock<std::mutex> &lock,
                           std::shared_ptr<Work> &&work);

    /*!
     * Cancel all work, notify worker thread.
     */
    void cancel_all_work();

    /*!
     * Thread main function.
     */
    static void worker(WorkQueue *q);
};

}

#endif /* !DBUS_ASYNC_WORKQUEUE_HH */
