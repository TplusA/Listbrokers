/*
 * Copyright (C) 2018  T+A elektroakustik GmbH & Co. KG
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

#ifndef SERVERS_LOST_AND_FOUND_HH
#define SERVERS_LOST_AND_FOUND_HH

#include <map>
#include <deque>
#include <string>
#include <algorithm>

#include <gio/gio.h>

namespace UPnP
{

/*!
 * Keep track of UPnP servers lost and found.
 *
 * For each D-Bus object corresponding to a UPnP server exported by dLeyna, a
 * queue of server-added "events" is stored. Each such event is a GCancellable
 * object corresponding to an asynchronous retrieval of a D-Bus proxy to the
 * corresponding server, plus a serial number to avoid certain race conditions
 * in case a server is found and lost in rapid succession.
 *
 * Whenever a server is found and a D-Bus proxy is about to be created
 * asynchronously, a GCancellable should be created by calling
 * #UPnP::ServersLostAndFound::server_found() and passed to the asynchronous
 * proxy creating function. Whenever a server is lost, it should be marked as
 * such by a call of #UPnP::ServersLostAndFound::server_lost().
 *
 * The callback function which handles asynchronous proxy creation must call
 * #UPnP::ServersLostAndFound::server_processed(), regardless of success.
 *
 * Note that the whole point of having this class around to be able to cancel
 * connections so that misbehaving UPnP servers cannot exhaust our memory
 * (hopefully).
 */
class ServersLostAndFound
{
  public:
    using Cancellable = std::unique_ptr<GCancellable, decltype(&g_object_unref)>;

    class AddToListData
    {
      public:
        const unsigned int serial_;

      private:
        using Cancellable = std::unique_ptr<GCancellable, decltype(&g_object_unref)>;
        Cancellable cancellable_;

      public:
        AddToListData(const AddToListData &) = delete;
        AddToListData &operator=(const AddToListData &) = delete;

        explicit AddToListData(unsigned int serial):
            serial_(serial),
            cancellable_(Cancellable(g_cancellable_new(), g_object_unref))
        {}

        bool cancel()
        {
            if(g_cancellable_is_cancelled(cancellable_.get()))
                return false;

            g_cancellable_cancel(cancellable_.get());

            return true;
        }

        GCancellable *get_cancellable() const { return cancellable_.get(); }
    };

  private:
    unsigned int next_serial_;
    std::map<std::string, std::deque<std::shared_ptr<AddToListData>>> server_added_queues_;

  public:
    ServersLostAndFound(const ServersLostAndFound &) = delete;
    ServersLostAndFound &operator=(const ServersLostAndFound &) = delete;

    explicit ServersLostAndFound():
        next_serial_(0)
    {}

    std::shared_ptr<const AddToListData> server_found(const std::string &object_path)
    {
        auto &queue = server_added_queues_[object_path];

        if(!queue.empty())
            queue.back()->cancel();

        queue.emplace_back(std::make_shared<AddToListData>(next_serial_++));

        return queue.back();
    }

    bool server_lost(const std::string &object_path)
    {
        auto it = server_added_queues_.find(object_path);

        if(it == server_added_queues_.end() || it->second.empty())
            return false;
        else
            return it->second.back()->cancel();
    }

    void server_processed(const std::string &object_path,
                          const AddToListData &data)
    {
        auto it = server_added_queues_.find(object_path);

        if(it == server_added_queues_.end())
        {
            BUG("Processed UPnP server %s (serial %u), but server unknown",
                object_path.c_str(), data.serial_);
            return;
        }

        auto &queue = it->second;

        if(queue.empty())
        {
            BUG("Processed UPnP server %s (serial %u), but queue empty",
                object_path.c_str(), data.serial_);
            server_added_queues_.erase(it);
            return;
        }

        const auto &add_data(queue.front()->serial_ == data.serial_
                             ? queue.begin()
                             : std::find_if(queue.begin() + 1, queue.end(),
                                            [&data]
                                            (const std::shared_ptr<AddToListData> &d)
                                            {
                                                return d->serial_ == data.serial_;
                                            }));

        if(add_data == queue.begin())
            queue.pop_front();
        else
        {
            BUG("UPnP server %s with serial %u not %sin queue",
                object_path.c_str(), data.serial_,
                add_data != queue.end() ? "first " : "");
            queue.erase(add_data);
        }

        if(queue.empty())
            server_added_queues_.erase(it);
    }
};

}

#endif /* !SERVERS_LOST_AND_FOUND_HH */
