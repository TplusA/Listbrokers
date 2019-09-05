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

#include <cstring>

#include "upnp_listtree.hh"
#include "listtree_glue.hh"
#include "dbus_artcache_iface_deep.h"

constexpr const char UPnP::ListTree::CONTEXT_ID[];

void UPnP::ListTree::add_to_server_list(const std::vector<std::string> &list)
{
    auto server_list = lt_manager_.lookup_list<UPnP::ServerList>(server_list_id_);
    log_assert(server_list != nullptr);

    for(auto &server_object_name : list)
        server_list->add_to_list(server_object_name,
                                 [this] () { reinsert_server_list(); });
}

void UPnP::ListTree::remove_from_server_list(const std::vector<std::string> &list)
{
    auto server_list = lt_manager_.lookup_list<UPnP::ServerList>(server_list_id_);
    log_assert(server_list != nullptr);

    use_list(server_list_id_, false);

    bool list_changed = false;

    for(auto &server_object_name : list)
    {
        ID::List removed_list_id;

        switch(server_list->remove_from_list(server_object_name, removed_list_id))
        {
          case ServerList::RemoveFromListResult::REMOVED:
            break;

          case ServerList::RemoveFromListResult::NOT_ADDED_YET:
            /* removed before ending up in our list */
            msg_info("Server %s removed while waiting for it",
                     server_object_name.c_str());
            continue;

          case ServerList::RemoveFromListResult::NOT_FOUND:
            /* removed something unknown */
            msg_error(0, LOG_NOTICE,
                      "Lost server %s, but is not in list",
                      server_object_name.c_str());
            continue;
        }

        list_changed = true;

        if(!removed_list_id.is_valid())
            continue;

        switch(lt_manager_.purge_subtree(removed_list_id, ID::List(), nullptr))
        {
          case ListTreeManager::PurgeResult::UNTOUCHED:
          case ListTreeManager::PurgeResult::PURGED:
            break;

          case ListTreeManager::PurgeResult::REPLACED_ROOT:
          case ListTreeManager::PurgeResult::PURGED_AND_REPLACED:
            BUG("%s(%d): unreachable", __func__, __LINE__);
            break;

          case ListTreeManager::PurgeResult::INVALID:
            msg_error(0, LOG_NOTICE,
                      "Purging subtree %u for server %s failed",
                      removed_list_id.get_raw_id(),
                      server_object_name.c_str());
            break;
        }
    }

    if(list_changed)
        reinsert_server_list();
}

void UPnP::ListTree::clear()
{
    auto server_list = lt_manager_.lookup_list<UPnP::ServerList>(server_list_id_);
    log_assert(server_list != nullptr);

    std::vector<std::string> list;
    std::transform(
        server_list->begin(), server_list->end(), std::back_inserter(list),
        [] (const auto &item)
        {
            return item.get_specific_data().get_dbus_path_copy();
        });

    remove_from_server_list(list);
}

void UPnP::ListTree::dump_server_list()
{
    auto all_servers = get_server_list();

    msg_info("Found %zu UPnP servers", all_servers->size());

    for(auto &server : *all_servers)
    {
        std::string name;
        server.get_name(name);

        msg_info("UPnP server: \"%s\"", name.c_str());
    }
}

ID::List UPnP::ListTree::enter_child(ID::List list_id, ID::Item item_id,
                                     ListError &error)
{
    if(list_id == server_list_id_)
        return lt_manager_.enter_child<UPnP::ServerList, UPnP::ItemData>(list_id, item_id, may_continue_fn_, error);
    else
        return lt_manager_.enter_child<UPnP::MediaList, UPnP::ItemData>(list_id, item_id, may_continue_fn_, error);
}

template <typename T>
static bool for_each_item_generic_apply_fn(ID::Item item_id, T &item,
                                           const ListTreeIface::ForEachGenericCallback &callback)
{
    ListTreeIface::ForEachItemDataGeneric data(item.get_kind());

    item.get_name(data.name_);

    return callback(data);
}

template <typename T>
static bool for_each_item_detailed_apply_fn(ID::Item item_id, T &item,
                                            const ListTreeIface::ForEachDetailedCallback &callback)
{
    std::string temp;
    item.get_name(temp);

    const ListTreeIface::ForEachItemDataDetailed data(temp, item.get_kind());

    return callback(data);
}

ListError UPnP::ListTree::for_each(ID::List list_id, ID::Item first, size_t count,
                                   const ListTreeIface::ForEachGenericCallback &callback) const
{
    if(list_id == server_list_id_)
    {
        const std::function<bool(ID::Item, const ListItem_<UPnP::ServerItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<UPnP::ServerItemData> &item)
            {
                return for_each_item_generic_apply_fn<const ListItem_<UPnP::ServerItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const UPnP::ServerList>(list_id),
                               first, count, fn);
    }
    else
    {
        const std::function<bool(ID::Item, const ListItem_<UPnP::ItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<UPnP::ItemData> &item)
            {
                return for_each_item_generic_apply_fn<const ListItem_<UPnP::ItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const UPnP::MediaList>(list_id),
                               first, count, fn);
    }
}

ListError UPnP::ListTree::for_each(ID::List list_id, ID::Item first, size_t count,
                                   const ListTreeIface::ForEachDetailedCallback &callback) const
{
    if(list_id == server_list_id_)
    {
        const std::function<bool(ID::Item, const ListItem_<UPnP::ServerItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<UPnP::ServerItemData> &item)
            {
                return for_each_item_detailed_apply_fn<const ListItem_<UPnP::ServerItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const UPnP::ServerList>(list_id),
                               first, count, fn);
    }
    else
    {
        const std::function<bool(ID::Item, const ListItem_<UPnP::ItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<UPnP::ItemData> &item)
            {
                return for_each_item_detailed_apply_fn<const ListItem_<UPnP::ItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const UPnP::MediaList>(list_id),
                               first, count, fn);
    }
}

ssize_t UPnP::ListTree::size(ID::List list_id) const
{
    if(list_id == server_list_id_)
    {
        const auto list = lt_manager_.lookup_list<const UPnP::ServerList>(list_id);

        if(list != nullptr)
            return list->size();
    }
    else
    {
        const auto list = lt_manager_.lookup_list<const UPnP::MediaList>(list_id);

        if(list != nullptr)
            return list->size();
    }

    return -1;
}

ID::List UPnP::ListTree::get_parent_link(ID::List list_id, ID::Item &parent_item_id) const
{
    const auto list = lt_manager_.lookup_list<const LRU::Entry>(list_id);

    if(list == nullptr)
        return ID::List();

    const auto parent = list->get_parent();

    if(parent == nullptr)
        return list_id;

    bool ok =
        (parent->get_cache_id() == server_list_id_)
        ? std::static_pointer_cast<const UPnP::ServerList>(parent)->lookup_item_id_by_child_id(list_id, parent_item_id)
        : std::static_pointer_cast<const UPnP::MediaList>(parent)->lookup_item_id_by_child_id(list_id, parent_item_id);

    if(ok)
        return parent->get_cache_id();

    BUG("Failed to find item in list %u linking to child list %u",
        parent->get_cache_id().get_raw_id(), list_id.get_raw_id());

    return ID::List();
}

ID::List UPnP::ListTree::get_link_to_context_root_impl(const char *context_id,
                                                       ID::Item &item_id,
                                                       bool &context_is_known,
                                                       bool &context_has_parent)
{
    context_is_known = (strcmp(context_id, CONTEXT_ID) == 0);
    return ID::List();
}

const ListItem_<UPnP::ServerItemData> *
UPnP::ListTree::get_server_item(const UPnP::MediaList &list) const
{
    const LRU::Entry *e = &list;
    ID::List child_list_id;

    do
    {
        child_list_id = e->get_cache_id();
        e = e->get_parent().get();

        if(e == nullptr)
        {
            BUG("No UPnP server for list %u, cache corrupt",
                list.get_cache_id().get_raw_id());
            return nullptr;
        }
    }
    while(e->get_cache_id() != server_list_id_);

    ID::Item item_idx;

    if(!static_cast<const UPnP::ServerList *>(e)->lookup_item_id_by_child_id(child_list_id, item_idx))
    {
        BUG("UPnP server for list %u not found",
            list.get_cache_id().get_raw_id());
        return nullptr;
    }

    auto servers = static_cast<const UPnP::ServerList *>(e);

    return &(*servers)[item_idx];
}

/*!
 * \todo This function uses the org.gnome.UPnP.MediaItem2.URLs property, but
 *     that interface makes is practically impossible to determine MIME types
 *     of streams as soon as there are multiple URLs to choose from (see
 *     https://01.org/dleyna/documentation/dleyna-server/item-objects). dLeyna
 *     defines the "Resources" property which should be used to handle this
 *     case cleanly.
 */
ListError UPnP::ListTree::get_uris_for_item(ID::List list_id, ID::Item item_id,
                                            std::vector<Url::String> &uris,
                                            ListItemKey &item_key) const
{
    uris.clear();

    if(list_id == server_list_id_)
        return ListError(ListError::INVALID_ID);

    const auto list = lt_manager_.lookup_list<const UPnP::MediaList>(list_id);

    if(list == nullptr)
        return ListError(ListError::INVALID_ID);

    if(item_id.get_raw_id() >= list->size())
        return ListError(ListError::INVALID_ID);

    const auto &item = (*list)[item_id];

    if(item.get_kind().is_directory())
        return ListError();

    const auto &dbus_path(item.get_specific_data().get_dbus_path());

    tdbusupnpMediaItem2 *proxy =
        create_media_item_proxy_for_object_path(dbus_path.c_str());

    if(proxy == nullptr)
        return ListError(ListError::NOT_FOUND);

    MD5::Context ctx;
    MD5::init(ctx);
    MD5::update(ctx,
                static_cast<const uint8_t *>(static_cast<const void *>(dbus_path.c_str())),
                dbus_path.size());
    MD5::finish(ctx, item_key.get_for_setting());

    send_cover_art(item, item_key, 100);

    const gchar *const *uris_from_dbus = tdbus_upnp_media_item2_get_urls(proxy);

    if(uris_from_dbus == nullptr)
        msg_error(0, LOG_NOTICE,
                  "No URLs for item %u in list %u, D-Bus object %s",
                  list_id.get_raw_id(), item_id.get_raw_id(), dbus_path.c_str());
    else
    {
        for(/* nothing */; *uris_from_dbus != NULL; ++uris_from_dbus)
            uris.emplace_back(Url::String(Url::Sensitivity::GENERIC,
                                          *uris_from_dbus));
    }

    g_object_unref(proxy);

    return ListError();
}

bool UPnP::ListTree::can_handle_strbo_url(const std::string &url) const
{
    BUG("%s(): not implemented", __PRETTY_FUNCTION__);
    return false;
}

ListError UPnP::ListTree::realize_strbo_url(const std::string &url,
                                            ListTreeIface::RealizeURLResult &result)
{
    BUG("%s(): not implemented", __PRETTY_FUNCTION__);
    return ListError(ListError::INTERNAL);
}

std::unique_ptr<Url::Location>
UPnP::ListTree::get_location_key(ID::List list_id, ID::RefPos item_pos,
                                 bool as_reference_key, ListError &error) const
{
    BUG("%s(): not implemented", __PRETTY_FUNCTION__);
    return nullptr;
}

std::unique_ptr<Url::Location>
UPnP::ListTree::get_location_trace(ID::List list_id, ID::RefPos item_pos,
                                   ID::List ref_list_id, ID::RefPos ref_item_pos,
                                   ListError &error) const
{
    BUG("%s(): not implemented", __PRETTY_FUNCTION__);
    return nullptr;
}
