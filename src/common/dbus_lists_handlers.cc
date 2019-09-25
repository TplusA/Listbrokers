/*
 * Copyright (C) 2015, 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_lists_handlers.hh"
#include "dbus_lists_iface_deep.h"
#include "ranked_stream_links.hh"
#include "listtree_glue.hh"
#include "messages.h"
#include "gvariantwrapper.hh"

#include <future>

/*!
 * Base class template for all work done by \c de.tahifi.Lists.Navigation
 * methods.
 */
template <typename RT>
class NavListsWork: public DBusAsync::Work
{
  protected:
    using ResultType = RT;

    ListTreeIface &listtree_;
    std::promise<ResultType> promise_;
    std::future<ResultType> future_;

  private:
    bool cancellation_requested_;

  protected:
    explicit NavListsWork(ListTreeIface &listtree):
        listtree_(listtree),
        promise_(std::promise<ResultType>()),
        future_(promise_.get_future()),
        cancellation_requested_(false)
    {}

  public:
    NavListsWork(NavListsWork &&) = default;
    NavListsWork &operator=(NavListsWork &&) = default;

    virtual ~NavListsWork()
    {
        if(cancellation_requested_)
            listtree_.pop_cancel_blocking_operation();
    }

    auto &wait()
    {
        future_.wait();
        return future_;
    }

    void do_cancel(std::unique_lock<std::mutex> &lock) final override
    {
        if(cancellation_requested_)
        {
            BUG("Multiple cancellation requests");
            return;
        }

        cancellation_requested_ = true;
        listtree_.push_cancel_blocking_operation();
    }
};

constexpr const char *ListError::names_[];

const std::string ListTreeIface::empty_string;

static void enter_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.Lists.Navigation";

    msg_vinfo(MESSAGE_LEVEL_TRACE,
              "%s method invocation from '%s': %s",
              iface_name, g_dbus_method_invocation_get_sender(invocation),
              g_dbus_method_invocation_get_method_name(invocation));
}

gboolean DBusNavlists::get_list_contexts(tdbuslistsNavigation *object,
                                         GDBusMethodInvocation *invocation,
                                         IfaceData *data)
{
    enter_handler(invocation);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));

    data->listtree_.for_each_context(
        [&builder]
        (const char *context_id, const char *description, bool is_root)
        {
            g_variant_builder_add(&builder, "(ss)", context_id, description);
        });

    GVariant *contexts_variant = g_variant_builder_end(&builder);

    tdbus_lists_navigation_complete_get_list_contexts(object, invocation, contexts_variant);

    return TRUE;
}

class GetRange: public NavListsWork<std::tuple<ListError, ID::Item, GVariantWrapper>>
{
  public:
    static constexpr const char *const DBUS_RETURN_TYPE_STRING = "a(sy)";
    static constexpr const char *const DBUS_ELEMENT_TYPE_STRING = "(sy)";

  private:
    const ID::List list_id_;
    const ID::Item first_item_id_;
    const size_t count_;

  public:
    GetRange(GetRange &&) = default;
    GetRange &operator=(GetRange &&) = default;

    explicit GetRange(ListTreeIface &listtree, ID::List list_id,
                      ID::Item first_item_id, size_t count):
        NavListsWork(listtree),
        list_id_(list_id),
        first_item_id_(first_item_id),
        count_(count)
    {
        log_assert(list_id_.is_valid());
    }

  protected:
    bool do_run() final override
    {
        listtree_.use_list(list_id_, false);

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE(DBUS_RETURN_TYPE_STRING));

        const ListError error =
            listtree_.for_each(list_id_, first_item_id_, count_,
                [&builder] (const ListTreeIface::ForEachItemDataGeneric &item_data)
                {
                    msg_info("for_each(): %s, %s dir", item_data.name_.c_str(),
                             item_data.kind_.is_directory() ? "is" : "no");
                    g_variant_builder_add(&builder, DBUS_ELEMENT_TYPE_STRING,
                                          item_data.name_.c_str(),
                                          item_data.kind_.get_raw_code());
                    return true;
                });

        GVariant *items_in_range = g_variant_builder_end(&builder);

        if(error.failed())
        {
            g_variant_unref(items_in_range);
            items_in_range = g_variant_new(DBUS_RETURN_TYPE_STRING, NULL);
        }

        promise_.set_value(
            std::make_tuple(error, error.failed() ? ID::Item() : first_item_id_,
                            GVariantWrapper(items_in_range)));
        return error != ListError::INTERRUPTED;
    }
};

gboolean DBusNavlists::get_range(tdbuslistsNavigation *object,
                                 GDBusMethodInvocation *invocation,
                                 guint list_id, guint first_item_id,
                                 guint count, IfaceData *data)
{
    enter_handler(invocation);

    if(list_id == 0)
    {
        GVariant *const empty_range =
            g_variant_new(GetRange::DBUS_RETURN_TYPE_STRING, NULL);

        tdbus_lists_navigation_complete_get_range(object, invocation,
                                                  ListError::INVALID_ID,
                                                  0, empty_range);
        return TRUE;
    }

    GetRange w(data->listtree_, ID::List(list_id), ID::Item(first_item_id), count);
    auto &f(w.wait());
    auto result(f.get());

    tdbus_lists_navigation_complete_get_range(
            object, invocation, std::get<0>(result).get_raw_code(),
            std::get<1>(result).get_raw_id(),
            GVariantWrapper::move(std::get<2>(result)));

    return TRUE;
}

gboolean DBusNavlists::get_range_with_meta_data(tdbuslistsNavigation *object,
                                                GDBusMethodInvocation *invocation,
                                                guint list_id,
                                                guint first_item_id,
                                                guint count, IfaceData *data)
{
    enter_handler(invocation);

    ID::List id(list_id);
    ListError error;

    data->listtree_.use_list(id, false);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sssyy)"));

    if(!id.is_valid())
        error = ListError::INVALID_ID;
    else
        error =
            data->listtree_.for_each(id, ID::Item(first_item_id), count,
                [&builder] (const ListTreeIface::ForEachItemDataDetailed &item_data)
                {
                    msg_info("for_each(): \"%s\"/\"%s\"/\"%s\", primary %u, %s dir",
                             item_data.artist_.c_str(),
                             item_data.album_.c_str(),
                             item_data.title_.c_str(),
                             item_data.primary_string_index_,
                             item_data.kind_.is_directory() ? "is" : "no");
                    g_variant_builder_add(&builder, "(sssyy)",
                                          item_data.artist_.c_str(),
                                          item_data.album_.c_str(),
                                          item_data.title_.c_str(),
                                          item_data.primary_string_index_,
                                          item_data.kind_.get_raw_code());
                    return true;
                });

    GVariant *items_in_range = g_variant_builder_end(&builder);

    if(error.failed())
    {
        first_item_id = 0;
        g_variant_unref(items_in_range);
        items_in_range = g_variant_new("a(sssyy)", NULL);
    }

    tdbus_lists_navigation_complete_get_range_with_meta_data(object,
                                                             invocation,
                                                             error.get_raw_code(),
                                                             first_item_id,
                                                             items_in_range);

    return TRUE;
}

gboolean DBusNavlists::check_range(tdbuslistsNavigation *object,
                                   GDBusMethodInvocation *invocation,
                                   guint list_id, guint first_item_id,
                                   guint count, IfaceData *data)
{
    enter_handler(invocation);

    ID::List id(list_id);
    ssize_t number_of_items;

    data->listtree_.use_list(id, false);

    if(id.is_valid() && (number_of_items = data->listtree_.size(id)) >= 0)
    {
        if(size_t(number_of_items) >= first_item_id)
            number_of_items -= first_item_id;
        else
            number_of_items = 0;

        if(count > 0 && size_t(number_of_items) > count)
            number_of_items = count;

        tdbus_lists_navigation_complete_check_range(object, invocation, 0,
                                                    first_item_id, number_of_items);
    }
    else
    {
        static constexpr ListError error(ListError::INVALID_ID);
        tdbus_lists_navigation_complete_check_range(object, invocation,
                                                    error.get_raw_code(), 0, 0);
    }

    return TRUE;
}

class GetListID: public NavListsWork<std::tuple<ListError, ID::List, I18n::String>>
{
  private:
    const ID::List list_id_;
    const ID::Item item_id_;

  public:
    GetListID(GetListID &&) = default;
    GetListID &operator=(GetListID &&) = default;

    explicit GetListID(ListTreeIface &listtree,
                       ID::List list_id, ID::Item item_id):
        NavListsWork(listtree),
        list_id_(list_id),
        item_id_(item_id)
    {}

  protected:
    bool do_run() final override
    {
        if(list_id_.is_valid())
        {
            listtree_.use_list(list_id_, false);

            ListError error;
            const auto child_id = listtree_.enter_child(list_id_, item_id_, error);

            if(child_id.is_valid())
                promise_.set_value(std::make_tuple(
                    error, child_id,
                    listtree_.get_child_list_title(list_id_, item_id_)));
            else
                promise_.set_value(std::make_tuple(
                    error, child_id, I18n::String(false)));

            return error != ListError::INTERRUPTED;
        }

        const ID::List root_list_id(listtree_.get_root_list_id());

        if(root_list_id.is_valid())
        {
            listtree_.use_list(root_list_id, false);
            promise_.set_value(std::make_tuple(
                ListError(), root_list_id, listtree_.get_list_title(root_list_id)));
        }
        else
            promise_.set_value(std::make_tuple(
                ListError(), root_list_id, I18n::String(false)));

        return true;
    }
};

gboolean DBusNavlists::get_list_id(tdbuslistsNavigation *object,
                                   GDBusMethodInvocation *invocation,
                                   guint list_id, guint item_id, IfaceData *data)
{
    enter_handler(invocation);

    if(list_id == 0 && item_id != 0)
    {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Invalid combination of list ID and item ID");
        return TRUE;
    }

    GetListID w(data->listtree_, ID::List(list_id), ID::Item(item_id));
    auto &f(w.wait());
    auto result(f.get());

    tdbus_lists_navigation_complete_get_list_id(
            object, invocation, std::get<0>(result).get_raw_code(),
            std::get<1>(result).get_raw_id(),
            std::get<2>(result).get_text().c_str(),
            std::get<2>(result).is_translatable());

    return TRUE;
}

gboolean DBusNavlists::get_parameterized_list_id(tdbuslistsNavigation *object,
                                                 GDBusMethodInvocation *invocation,
                                                 guint list_id, guint item_id,
                                                 const gchar *parameter,
                                                 IfaceData *data)
{
    enter_handler(invocation);

    if(list_id == 0)
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Root lists are not parameterized");
    else
    {
        data->listtree_.use_list(ID::List(list_id), false);

        ListError error;
        const auto child_id =
            data->listtree_.enter_child_with_parameters(ID::List(list_id),
                                                        ID::Item(item_id),
                                                        parameter, error);

        if(child_id.is_valid())
        {
            const auto title(data->listtree_.get_child_list_title(ID::List(list_id), ID::Item(item_id)));

            tdbus_lists_navigation_complete_get_parameterized_list_id(object, invocation,
                                                                      error.get_raw_code(),
                                                                      child_id.get_raw_id(),
                                                                      title.get_text().c_str(),
                                                                      title.is_translatable());
        }
        else
            tdbus_lists_navigation_complete_get_parameterized_list_id(object, invocation,
                                                                      error.get_raw_code(),
                                                                      child_id.get_raw_id(),
                                                                      "", false);
    }

    return TRUE;
}

gboolean DBusNavlists::get_parent_link(tdbuslistsNavigation *object,
                                       GDBusMethodInvocation *invocation,
                                       guint list_id, IfaceData *data)
{
    enter_handler(invocation);

    data->listtree_.use_list(ID::List(list_id), false);

    ID::Item parent_item;
    const ID::List parent_list =
        data->listtree_.get_parent_link(ID::List(list_id), parent_item);

    if(parent_list.is_valid())
    {
        const guint ret_list =
            (parent_list.get_raw_id() != list_id) ? parent_list.get_raw_id() : 0;
        const guint ret_item =
            (ret_list != 0) ? parent_item.get_raw_id() : 1;
        const auto title(data->listtree_.get_list_title(parent_list));

        tdbus_lists_navigation_complete_get_parent_link(object, invocation,
                                                        ret_list, ret_item,
                                                        title.get_text().c_str(),
                                                        title.is_translatable());
    }
    else
        tdbus_lists_navigation_complete_get_parent_link(object, invocation,
                                                        0, 0, "", false);

    return TRUE;
}

gboolean DBusNavlists::get_root_link_to_context(tdbuslistsNavigation *object,
                                                GDBusMethodInvocation *invocation,
                                                const gchar *context,
                                                IfaceData *data)
{
    ID::Item item_id;
    bool context_is_known;
    bool context_has_parent;

    const ID::List list_id(data->listtree_.get_link_to_context_root(context, item_id,
                                                                    context_is_known,
                                                                    context_has_parent));

    if(!list_id.is_valid())
    {
        if(!context_is_known)
            g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                                  G_DBUS_ERROR_NOT_SUPPORTED,
                                                  "Context \"%s\" unknown",
                                                  context);
        else if(!context_has_parent)
            g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                                  G_DBUS_ERROR_INVALID_ARGS,
                                                  "Context \"%s\" has no parent",
                                                  context);
        else
            g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                                  G_DBUS_ERROR_FILE_NOT_FOUND,
                                                  "Context \"%s\" has no list",
                                                  context);

        return TRUE;
    }

    const auto title(data->listtree_.get_child_list_title(list_id, item_id));

    tdbus_lists_navigation_complete_get_root_link_to_context(object, invocation,
                                                             list_id.get_raw_id(),
                                                             item_id.get_raw_id(),
                                                             title.get_text().c_str(),
                                                             title.is_translatable());

    return TRUE;
}

gboolean DBusNavlists::get_uris(tdbuslistsNavigation *object,
                                GDBusMethodInvocation *invocation,
                                guint list_id, guint item_id, IfaceData *data)
{
    enter_handler(invocation);

    data->listtree_.use_list(ID::List(list_id), true);

    std::vector<Url::String> uris;
    ListItemKey item_key;
    ListError error =
        data->listtree_.get_uris_for_item(ID::List(list_id), ID::Item(item_id),
                                          uris, item_key);

    std::vector<const gchar *> list_of_uris_for_dbus;

    if(!error.failed())
        std::transform(uris.begin(), uris.end(),
                       std::back_inserter(list_of_uris_for_dbus),
                       [] (const auto &uri) { return uri.get_cleartext().c_str(); });

    list_of_uris_for_dbus.push_back(NULL);

    tdbus_lists_navigation_complete_get_uris(object, invocation,
                                             error.get_raw_code(),
                                             list_of_uris_for_dbus.data(),
                                             hash_to_variant(item_key));

    return TRUE;
}

gboolean DBusNavlists::get_ranked_stream_links(tdbuslistsNavigation *object,
                                               GDBusMethodInvocation *invocation,
                                               guint list_id, guint item_id,
                                               IfaceData *data)
{
    enter_handler(invocation);

    data->listtree_.use_list(ID::List(list_id), true);

    std::vector<Url::RankedStreamLinks> links;
    ListItemKey item_key;
    ListError error =
        data->listtree_.get_ranked_links_for_item(ID::List(list_id), ID::Item(item_id),
                                                  links, item_key);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(uus)"));

    if(!error.failed())
    {
        for(const auto &l : links)
            g_variant_builder_add(&builder, "(uus)",
                                  l.get_rank(), l.get_bitrate(),
                                  l.get_stream_link().url_.get_cleartext().c_str());
    }

    GVariant *list_of_links_for_dbus = g_variant_builder_end(&builder);

    tdbus_lists_navigation_complete_get_ranked_stream_links(object, invocation,
                                                            error.get_raw_code(),
                                                            list_of_links_for_dbus,
                                                            hash_to_variant(item_key));

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.DiscardList().
 */
gboolean DBusNavlists::discard_list(tdbuslistsNavigation *object,
                                    GDBusMethodInvocation *invocation,
                                    guint list_id, IfaceData *data)
{
    enter_handler(invocation);

    data->listtree_.discard_list_hint(ID::List(list_id));

    tdbus_lists_navigation_complete_discard_list(object, invocation);
    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.KeepAlive().
 */
gboolean DBusNavlists::keep_alive(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  GVariant *list_ids, IfaceData *data)
{
    enter_handler(invocation);

    GVariantIter iter;
    g_variant_iter_init(&iter, list_ids);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));

    guint raw_list_id;
    while(g_variant_iter_loop(&iter,"u", &raw_list_id))
    {
        if(!data->listtree_.use_list(ID::List(raw_list_id), false))
        {
            msg_error(0, LOG_NOTICE,
                      "List %u is invalid, cannot keep it alive", raw_list_id);
            g_variant_builder_add(&builder, "u", raw_list_id);
        }
    }

    GVariant *invalid_list_ids = g_variant_builder_end(&builder);

    const guint64 gc_interval =
        std::chrono::duration_cast<std::chrono::milliseconds>(data->listtree_.get_gc_expiry_time()).count();

    tdbus_lists_navigation_complete_keep_alive(object, invocation,
                                               gc_interval, invalid_list_ids);

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.ForceInCache().
 */
gboolean DBusNavlists::force_in_cache(tdbuslistsNavigation *object,
                                      GDBusMethodInvocation *invocation,
                                      guint list_id, gboolean force,
                                      IfaceData *data)
{
    enter_handler(invocation);

    const auto id = ID::List(list_id);

    if(id.is_valid())
    {
        const auto list_expiry_ms(std::chrono::milliseconds(
                        data->listtree_.force_list_into_cache(id, force)));

        tdbus_lists_navigation_complete_force_in_cache(object, invocation,
                                                       list_expiry_ms.count());
    }
    else
        tdbus_lists_navigation_complete_force_in_cache(object, invocation, 0);

    return TRUE;
}

gboolean DBusNavlists::get_location_key(tdbuslistsNavigation *object,
                                        GDBusMethodInvocation *invocation,
                                        guint list_id, guint item_id,
                                        gboolean as_reference_key,
                                        IfaceData *data)
{
    enter_handler(invocation);

    const auto id = ID::List(list_id);
    ListError error;

    if(id.is_valid())
    {
        if(as_reference_key && item_id == 0)
            error = ListError::NOT_SUPPORTED;

        std::unique_ptr<Url::Location> location = error.failed()
            ? nullptr
            : data->listtree_.get_location_key(id, ID::RefPos(item_id),
                                               as_reference_key, error);

        if(location != nullptr)
        {
            tdbus_lists_navigation_complete_get_location_key(object, invocation,
                                                             error.get_raw_code(),
                                                             location->str().c_str());
            return TRUE;
        }
    }
    else
        error = ListError::INVALID_ID;

    tdbus_lists_navigation_complete_get_location_key(object, invocation,
                                                     error.get_raw_code(),
                                                     "");

    return TRUE;
}

gboolean DBusNavlists::get_location_trace(tdbuslistsNavigation *object,
                                          GDBusMethodInvocation *invocation,
                                          guint list_id, guint item_id,
                                          guint ref_list_id, guint ref_item_id,
                                          IfaceData *data)
{
    enter_handler(invocation);

    const auto obj_list_id = ID::List(list_id);
    ListError error;

    if(obj_list_id.is_valid())
    {
        if(item_id == 0 ||
           (ref_list_id != 0 && ref_item_id == 0) ||
           obj_list_id == ID::List(ref_list_id))
            error = ListError::NOT_SUPPORTED;
        else if(ref_list_id == 0 && ref_item_id != 0)
            error = ListError::INVALID_ID;

        std::unique_ptr<Url::Location> location = error.failed()
            ? nullptr
            : data->listtree_.get_location_trace(obj_list_id,
                                                 ID::RefPos(item_id),
                                                 ID::List(ref_list_id),
                                                 ID::RefPos(ref_item_id),
                                                 error);

        if(location != nullptr)
        {
            tdbus_lists_navigation_complete_get_location_trace(object, invocation,
                                                             error.get_raw_code(),
                                                             location->str().c_str());
            return TRUE;
        }
    }
    else
        error = ListError::INVALID_ID;

    tdbus_lists_navigation_complete_get_location_trace(object, invocation,
                                                       error.get_raw_code(),
                                                       "");

    return TRUE;
}

class RealizeLocation: public NavListsWork<std::tuple<ListError, ListTreeIface::RealizeURLResult>>
{
  private:
    const std::string url_;

  public:
    RealizeLocation(RealizeLocation &&) = default;
    RealizeLocation &operator=(RealizeLocation &&) = default;

    explicit RealizeLocation(ListTreeIface &listtree, std::string &&url):
        NavListsWork(listtree),
        url_(std::move(url))
    {
        log_assert(!url.empty());
    }

  protected:
    bool do_run() final override
    {
        ListTreeIface::RealizeURLResult result;
        const auto error = listtree_.realize_strbo_url(url_, result);
        promise_.set_value(std::make_tuple(error, std::move(result)));
        return error != ListError::INTERRUPTED;
    }
};

gboolean DBusNavlists::realize_location(tdbuslistsNavigation *object,
                                        GDBusMethodInvocation *invocation,
                                        const gchar *location_url,
                                        IfaceData *data)
{
    enter_handler(invocation);
    tdbus_lists_navigation_complete_realize_location(object, invocation,
                                                     ListError::INTERNAL, 0);
    return TRUE;
}

gboolean DBusNavlists::abort_realize_location(tdbuslistsNavigation *object,
                                              GDBusMethodInvocation *invocation,
                                              guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                          G_DBUS_ERROR_INVALID_ARGS,
                                          "Wrong cookie");

    return TRUE;
}
