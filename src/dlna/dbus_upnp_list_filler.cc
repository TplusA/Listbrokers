/*
 * Copyright (C) 2015--2017, 2019--2022  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_upnp_list_filler.hh"
#include "dbus_upnp_list_filler_helpers.hh"
#include "upnp_listtree.hh"
#include "main.hh"
#include "gerrorwrapper.hh"

static UPnP::DBusUPnPFiller standard_dbus_filler;

void UPnP::init_standard_dbus_fillers(const LRU::Cache &cache)
{
    standard_dbus_filler.init(cache);
}

namespace UPnP
{
    template <>
    const TiledListFillerIface<ItemData> &get_tiled_list_filler_for_root_directory()
    {
        return standard_dbus_filler;
    }
}

static ListError fill_list_item_from_upnp_data(UPnP::ItemData &&list_item,
                                               GVariant *child_data)
{
    GVariantIter iter;
    g_variant_iter_init(&iter, child_data);

    const gchar *display_name = NULL;
    const gchar *path = NULL;
    const gchar *album_art_url = NULL;
    bool is_container = false;
    bool is_container_set = false;

    const gchar *key;
    GVariant *value;
    while(g_variant_iter_loop(&iter,"{&sv}", &key, &value))
    {
        if(strcmp(key, "DisplayName") == 0)
            display_name = g_variant_get_string(value, NULL);
        else if(strcmp(key, "Path") == 0)
            path = g_variant_get_string(value, NULL);
        else if(strcmp(key, "AlbumArtURL") == 0)
            album_art_url = g_variant_get_string(value, NULL);
        else if(strcmp(key, "Type") == 0)
        {
            is_container_set = true;
            is_container = (strcmp(g_variant_get_string(value, NULL), "container") == 0);
        }
        else
            msg_error(E2BIG, LOG_NOTICE,
                      "Received unrequested information from UPnP "
                      "server (ListChildrenEx()): \"%s\" (ignored)",
                      key);
    }

    if(display_name != NULL && path != NULL && is_container_set)
    {
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "D-Bus subpath for \"%s\" is \"%s\"", display_name, path);
        list_item = UPnP::ItemData(path, display_name,
                                   album_art_url != nullptr
                                   ? Url::String(Url::Sensitivity::GENERIC,
                                                 album_art_url)
                                   : Url::String(Url::Sensitivity::GENERIC),
                                   is_container);
        return ListError();
    }

    msg_error(ENOMSG, LOG_NOTICE,
              "Malformed or incomplete DLNA child container information");

    return ListError(ListError::PROTOCOL);
}

static ListError io_error_to_list_error(const GErrorWrapper &gerror)
{
    if(gerror->domain == G_IO_ERROR)
    {
        switch(GIOErrorEnum(gerror->code))
        {
          case G_IO_ERROR_FAILED:
          case G_IO_ERROR_EXISTS:
          case G_IO_ERROR_IS_DIRECTORY:
          case G_IO_ERROR_NOT_DIRECTORY:
          case G_IO_ERROR_NOT_EMPTY:
          case G_IO_ERROR_NOT_REGULAR_FILE:
          case G_IO_ERROR_NOT_SYMBOLIC_LINK:
          case G_IO_ERROR_NOT_MOUNTABLE_FILE:
          case G_IO_ERROR_FILENAME_TOO_LONG:
          case G_IO_ERROR_INVALID_FILENAME:
          case G_IO_ERROR_TOO_MANY_LINKS:
          case G_IO_ERROR_NO_SPACE:
          case G_IO_ERROR_INVALID_ARGUMENT:
          case G_IO_ERROR_NOT_MOUNTED:
          case G_IO_ERROR_ALREADY_MOUNTED:
          case G_IO_ERROR_CLOSED:
          case G_IO_ERROR_READ_ONLY:
          case G_IO_ERROR_CANT_CREATE_BACKUP:
          case G_IO_ERROR_WRONG_ETAG:
          case G_IO_ERROR_TIMED_OUT:
          case G_IO_ERROR_WOULD_RECURSE:
          case G_IO_ERROR_WOULD_MERGE:
          case G_IO_ERROR_FAILED_HANDLED:
          case G_IO_ERROR_TOO_MANY_OPEN_FILES:
          case G_IO_ERROR_NOT_INITIALIZED:
          case G_IO_ERROR_ADDRESS_IN_USE:
          case G_IO_ERROR_PARTIAL_INPUT:
          case G_IO_ERROR_INVALID_DATA:
          case G_IO_ERROR_DBUS_ERROR:
          case G_IO_ERROR_BROKEN_PIPE:
          case G_IO_ERROR_NOT_CONNECTED:
#if GLIB_CHECK_VERSION(2, 48, 0)
          case G_IO_ERROR_MESSAGE_TOO_LARGE:
#endif /*  */
            return ListError(ListError::PROTOCOL);

          case G_IO_ERROR_NOT_FOUND:
            return ListError(ListError::EMPTY);

          case G_IO_ERROR_PERMISSION_DENIED:
          case G_IO_ERROR_CONNECTION_REFUSED:
          case G_IO_ERROR_PROXY_AUTH_FAILED:
          case G_IO_ERROR_PROXY_NEED_AUTH:
          case G_IO_ERROR_PROXY_NOT_ALLOWED:
            return ListError(ListError::PERMISSION_DENIED);

          case G_IO_ERROR_NOT_SUPPORTED:
            return ListError(ListError::NOT_SUPPORTED);

          case G_IO_ERROR_CANCELLED:
            return ListError(ListError::INTERRUPTED);

          case G_IO_ERROR_PENDING:
          case G_IO_ERROR_BUSY:
          case G_IO_ERROR_WOULD_BLOCK:
            return ListError(ListError::BUSY_3000);

          case G_IO_ERROR_HOST_NOT_FOUND:
          case G_IO_ERROR_HOST_UNREACHABLE:
          case G_IO_ERROR_NETWORK_UNREACHABLE:
          case G_IO_ERROR_PROXY_FAILED:
            return ListError(ListError::NET_IO);
        }
    }

    return ListError(ListError::PROTOCOL);
}

ssize_t UPnP::DBusUPnPFiller::fill(ItemProvider<UPnP::ItemData> &item_provider,
                                   ID::List list_id, ID::Item idx,
                                   size_t count, ListError &error,
                                   const std::function<bool()> &may_continue) const
{
    error = ListError::OK;

    log_assert(cache_ != nullptr);

    /*!
     * \bug There should be a hot D-Bus proxy object for the most recently
     *     accessed D-Bus object for a speed-up.
     */
    const auto media_list(std::static_pointer_cast<const UPnP::MediaList>(cache_->lookup(list_id)));

    tdbusupnpMediaContainer2 *proxy =
        create_media_container_proxy_for_object_path(media_list->get_dbus_object_path().c_str());

    if(proxy == nullptr)
    {
        msg_error(0, LOG_ERR, "Cannot fill list, dLeyna not up and running");
        error = ListError::NOT_FOUND;
        return -1;
    }

    GVariant *children = NULL;

    static const char *const filter_with_album_art[] =
    {
        "DisplayName",
        "Path",
        "Type",
        "AlbumArtURL",
        NULL
    };

    static const char *const filter_without_album_art[] =
    {
        "DisplayName",
        "Path",
        "Type",
        NULL
    };

    const auto *server =
        static_cast<const ListTree &>(LBApp::get_list_tree_data_singleton().get_list_tree()).get_server_item(*media_list);

    static constexpr ServerQuirks quirks(ServerQuirks::album_art_url_not_usable);
    const char *const *filter =
        (server != nullptr && server->get_specific_data().has_quirks(quirks))
        ? filter_without_album_art
        : filter_with_album_art;

    ssize_t retval;

    GErrorWrapper gerror;
    const gboolean success = request_alphabetically_sorted_
        ? tdbus_upnp_media_container2_call_list_children_ex_sync(proxy,
                                                                 idx.get_raw_id(),
                                                                 count, filter,
                                                                 "+DisplayName",
                                                                 &children,
                                                                 NULL, gerror.await())
        : tdbus_upnp_media_container2_call_list_children_sync(proxy,
                                                              idx.get_raw_id(),
                                                              count, filter,
                                                              &children,
                                                              NULL, gerror.await());

    if(success)
    {
        gsize num_of_children = g_variant_n_children(children);

        if(num_of_children > count)
        {
            msg_error(ERANGE, LOG_NOTICE,
                      "Got too many child elements from UPnP server "
                      "(requested %zu, got %zu), ignoring excess elements",
                      count, num_of_children);
            num_of_children = count;
        }

        for(retval = 0; !error.failed() && size_t(retval) < num_of_children; ++retval)
        {
            /* one output parameter per child: array of dictionaries of
             * string/variant pairs */
            GVariant *child_data = g_variant_get_child_value(children, retval);
            log_assert(child_data != nullptr);

            UPnP::ItemData *item = item_provider.next();
            error = fill_list_item_from_upnp_data(std::move(*item), child_data);

            g_variant_unref(child_data);
        }

        g_variant_unref(children);
    }
    else
    {
        msg_error(0, LOG_ERR, "List children failed");
        gerror.log_failure(request_alphabetically_sorted_
                           ? "Get list of UPnP children (sorted)"
                           : "Get list of UPnP children (unsorted)");
        retval = -1;
        error = io_error_to_list_error(gerror);
    }

    g_object_unref(proxy);

    return retval;
}
