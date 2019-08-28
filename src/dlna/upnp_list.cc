/*
 * Copyright (C) 2015, 2017, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <stack>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>

#include "upnp_list.hh"
#include "servers_lost_and_found.hh"

gpointer (*UPnP::ServerItemData::object_ref)(gpointer) = g_object_ref;
void (*UPnP::ServerItemData::object_unref)(gpointer) = g_object_unref;

template<>
ListThreads<UPnP::ItemData, UPnP::media_list_tile_size>
TiledList<UPnP::ItemData, UPnP::media_list_tile_size>::thread_pool(false);

void UPnP::ServerItemData::init(tdbusdleynaserverMediaDevice *dbus_proxy)
{
    dbus_proxy_ = dbus_proxy;
    object_ref(dbus_proxy_);

    server_quirks_ = ServerQuirks();

    const gchar *temp = tdbus_dleynaserver_media_device_get_model_name(dbus_proxy_);

    if(temp != nullptr && strcmp(temp, "MediaTomb") == 0)
    {
        /* MediaTomb sucks at processing cover art, so we disable it (#505) */
        server_quirks_.add(ServerQuirks::album_art_url_not_usable);
    }
}

UPnP::ServerItemData::~ServerItemData()
{
    if(dbus_proxy_)
    {
        object_unref(dbus_proxy_);
        dbus_proxy_ = nullptr;
    }
}

static bool name_is_ok(const gchar *name)
{
    return (name != nullptr && name[0] != '\0');
}

void UPnP::ServerItemData::get_name(std::string &name) const
{
    log_assert(dbus_proxy_ != nullptr);

    const gchar *temp = tdbus_dleynaserver_media_device_get_friendly_name(dbus_proxy_);

    if(name_is_ok(temp))
        name = temp;
    else
        name = "Nameless UPnP device";

    std::vector<const gchar *> fragments;

    temp = tdbus_dleynaserver_media_device_get_model_description(dbus_proxy_);
    if(name_is_ok(temp))
        fragments.push_back(temp);

    temp = tdbus_dleynaserver_media_device_get_model_name(dbus_proxy_);
    if(name_is_ok(temp))
        fragments.push_back(temp);

    temp = tdbus_dleynaserver_media_device_get_model_number(dbus_proxy_);
    if(name_is_ok(temp))
        fragments.push_back(temp);

    if(fragments.empty())
        return;

    name += " (";

    auto frag = fragments.begin();
    name += *frag;

    for(++frag; frag !=fragments.end(); ++frag)
    {
        name += ' ';
        name += *frag;
    }

    name += ")";
}

const I18n::String UPnP::ServerList::LIST_TITLE(true, "All UPnP servers");

void UPnP::ServerList::enumerate_tree_of_sublists(const LRU::Cache &cache,
                                                  std::vector<ID::List> &nodes,
                                                  bool append_to_nodes) const
{
    if(!append_to_nodes)
        nodes.clear();

    nodes.push_back(get_cache_id());

    for(const auto &server : (*this))
    {
        ID::List child_list_id = server.get_child_list();

        if(child_list_id.is_valid())
            cache.lookup(child_list_id)->enumerate_tree_of_sublists(cache, nodes, true);
    }
}

void UPnP::ServerList::enumerate_direct_sublists(const LRU::Cache &cache,
                                                 std::vector<ID::List> &nodes) const
{
    BUG("%s(): function shall not be called", __PRETTY_FUNCTION__);
}

void UPnP::ServerList::obliviate_child(ID::List child_id, const Entry *child)
{
    ID::Item idx;

    if(lookup_item_id_by_child_id(child_id, idx))
        (*this)[idx].obliviate_child();
    else
        BUG("Got obliviate notification for server root %u, "
            "but could not find it in server list (ID %u)",
            child_id.get_raw_id(), get_cache_id().get_raw_id());
}

class AddToListAsyncData
{
  public:
    UPnP::ServerList &server_list_;
    const std::string object_path_;
    const std::function<void()> notify_server_added_;
    const std::shared_ptr<const UPnP::ServersLostAndFound::AddToListData> data_;

    AddToListAsyncData(const AddToListAsyncData &) = delete;
    AddToListAsyncData &operator=(const AddToListAsyncData &) = delete;

    explicit AddToListAsyncData(UPnP::ServerList &server_list,
                                const std::string &object_path,
                                std::function<void()> &&notify_server_added,
                                std::shared_ptr<const UPnP::ServersLostAndFound::AddToListData> &&data):
        server_list_(server_list),
        object_path_(object_path),
        notify_server_added_(std::move(notify_server_added)),
        data_(std::move(data))
    {}
};

namespace UpnpServerListDetail  {
  static bool is_media_server_local(tdbusdleynaserverMediaDevice *proxy);
};

bool UpnpServerListDetail::is_media_server_local(tdbusdleynaserverMediaDevice *proxy)
{
    const auto *server_location_cstr = tdbus_dleynaserver_media_device_get_location(proxy);
    if(server_location_cstr == nullptr)
        return false;

    const std::string server_location(server_location_cstr);

    // Extract host part from location string

    std::string host_string;
    const std::string prot_end("://");
    std::string::const_iterator it1 = std::search(server_location.begin(), server_location.end(),
                                                  prot_end.begin(), prot_end.end());
    if(it1 != server_location.end())
    {
        advance(it1, prot_end.length());
        std::string::const_iterator it2 = std::find_if(it1, server_location.end(),
                                                       [](char c){ return c=='/'||c==':'; });
        host_string.reserve(distance(it1, it2));
        std::copy(it1, it2, back_inserter(host_string));
    }

    // Convert host from string to byte-representation

    in_addr host_addr;
    const int rc = inet_pton(AF_INET, host_string.c_str(), &host_addr);
    if(rc != 1)
    {
        msg_error(0, LOG_NOTICE, "Can't parse IP address: %s", host_string.c_str());
        return false;
    }

    // Iterate trough network interfaces and compare IP

    ifaddrs *ifaddr;
    if(getifaddrs(&ifaddr) != 0)
    {
        msg_error(0, LOG_NOTICE, "Can't list network interfaces");
        return false;
    }

    bool result = false;
    for(ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == NULL)
            continue;

        if(ifa->ifa_addr->sa_family == AF_INET)
        {
            const in_addr if_addr = (reinterpret_cast<sockaddr_in*>(ifa->ifa_addr))->sin_addr;
            if(if_addr.s_addr == host_addr.s_addr)
            {
                result = true;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

void UPnP::ServerList::media_device_proxy_connected(GObject *source_object,
                                                    GAsyncResult *res,
                                                    gpointer user_data)
{
    auto *data_ptr = static_cast<AddToListAsyncData *>(user_data);
    log_assert(data_ptr != nullptr);

    auto &data = *data_ptr;

    tdbusdleynaserverMediaDevice *proxy =
        UPnP::create_media_device_proxy_for_object_path_end(data.object_path_, res);

    if(proxy != nullptr)
    {
        auto it(std::find_if(data.server_list_.begin(), data.server_list_.end(),
                             [&data] (ListItem_<ServerItemData> &li)
                             {
                                 return proxy_object_path_equals(li.get_specific_data().get_dbus_proxy(),
                                                                 data.object_path_);
                             }));

        if(it != data.server_list_.end())
        {
            msg_info("Updating already known UPnP server %s", data.object_path_.c_str());
            it->get_specific_data().replace_dbus_proxy(proxy);
            if(data.notify_server_added_ != nullptr)
                data.notify_server_added_();
        }
        else if(UpnpServerListDetail::is_media_server_local(proxy))
            msg_error(0, LOG_NOTICE, "Ignoring UPnP server on the same host");
        else if(!UPnP::is_media_device_usable(proxy))
            msg_error(0, LOG_NOTICE,
                      "Ignoring UPnP server %s, seems to be unusable",
                      data.object_path_.c_str());
        else
        {
            ListItem_<ServerItemData> new_server;
            new_server.get_specific_data().init(proxy);
            data.server_list_.append_unsorted(std::move(new_server));

            if(data.notify_server_added_ != nullptr)
                data.notify_server_added_();
        }

        UPnP::ServerItemData::object_unref(proxy);
        proxy = nullptr;
    }

    log_assert(data.data_ != nullptr);
    data.server_list_.servers_lost_and_found_.server_processed(data.object_path_,
                                                               *data.data_);

    /* raw delete here because of raw new in #UPnP::ServerList::add_to_list(),
     * thanks to GLib standing right in our way */
    delete data_ptr;
}

void UPnP::ServerList::add_to_list(const std::string &object_path,
                                   std::function<void()> &&notify_server_added)
{
    auto data = servers_lost_and_found_.server_found(object_path);
    log_assert(data != nullptr);

    /* raw new here because of the C code path; matching delete to be found in
     * #UPnP::ServerList::media_device_proxy_connected() */
    auto *add_data = new AddToListAsyncData(*this, object_path,
                                            std::move(notify_server_added),
                                            std::move(data));
    if(add_data == nullptr)
        return;

    if(!UPnP::create_media_device_proxy_for_object_path_begin(
            object_path, add_data->data_->get_cancellable(),
            media_device_proxy_connected, add_data))
        delete add_data;
}

UPnP::ServerList::RemoveFromListResult
UPnP::ServerList::remove_from_list(const std::string &object_path,
                                   ID::List &child_list_id)
{
    const bool cancelled = servers_lost_and_found_.server_lost(object_path);

    auto it(std::find_if(this->begin(), this->end(),
                         [&object_path] (ListItem_<ServerItemData> &li)
                         {
                             return proxy_object_path_equals(li.get_specific_data().get_dbus_proxy(),
                                                             object_path);
                         }));

    if(it == this->end())
        return cancelled ? RemoveFromListResult::NOT_ADDED_YET : RemoveFromListResult::NOT_FOUND;

    const auto idx = std::distance(this->begin(), it);

    child_list_id = FIXME_remove(ID::Item(idx));

    return RemoveFromListResult::REMOVED;
}

void UPnP::MediaList::enumerate_direct_sublists(const LRU::Cache &cache,
                                                std::vector<ID::List> &nodes) const
{
    for(auto it = begin(); it != end(); ++it)
    {
        auto id = it->get_child_list();

        if(id.is_valid())
            nodes.push_back(id);
    }
}

void UPnP::MediaList::obliviate_child(ID::List child_id, const Entry *child)
{
    ID::Item idx;

    if(lookup_item_id_by_child_id(child_id, idx))
        (*this)[idx].obliviate_child();
    else
        BUG("Got obliviate notification for child %u, "
            "but could not find it in list with ID %u",
            child_id.get_raw_id(), get_cache_id(). get_raw_id());
}

template <typename T>
static inline std::string get_dbus_object_path(const UPnP::MediaList &list)
{
    auto item_list = std::static_pointer_cast<T>(list.get_parent());
    auto child_item = item_list->lookup_child_by_id(list.get_cache_id());
    log_assert(child_item != nullptr);

    return child_item->get_specific_data().get_dbus_path_copy();
}

std::string UPnP::MediaList::get_dbus_object_path() const
{
    log_assert(get_parent() != nullptr);
    log_assert(get_cache_id().is_valid());

    if(get_parent()->get_parent() != nullptr)
    {
        /* parent also has a parent, must also be a #UPnP::MediaList */
        return ::get_dbus_object_path<const UPnP::MediaList>(*this);
    }
    else
    {
        /* reached list of UPnP servers */
        return ::get_dbus_object_path<const UPnP::ServerList>(*this);
    }
}
