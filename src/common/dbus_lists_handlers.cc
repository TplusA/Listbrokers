/*
 * Copyright (C) 2015--2017, 2019--2023  T+A elektroakustik GmbH & Co. KG
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
#include "dbus_lists_iface.hh"
#include "ranked_stream_links.hh"
#include "listtree_glue.hh"
#include "work_by_cookie.hh"
#include "messages.h"
#include "gvariantwrapper.hh"

#include <unordered_map>
#include <future>

/*!
 * Base class template for all work done by \c de.tahifi.Lists.Navigation
 * methods.
 */
template <typename RT>
class NavListsWork: public DBusAsync::CookiedWorkWithFutureResultBase<RT>
{
  protected:
    ListTreeIface &listtree_;

  protected:
    explicit NavListsWork(const std::string &name, ListTreeIface &listtree):
        DBusAsync::CookiedWorkWithFutureResultBase<RT>(name),
        listtree_(listtree)
    {}

  public:
    NavListsWork(NavListsWork &&) = default;
    NavListsWork &operator=(NavListsWork &&) = default;

    virtual ~NavListsWork()
    {
        if(this->was_canceled())
            listtree_.pop_cancel_blocking_operation();
    }

    void notify_data_available(uint32_t cookie) const final override
    {
        tdbus_lists_navigation_emit_data_available(
            DBusNavlists::get_navigation_iface(),
            g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
                                      &cookie, 1, sizeof(cookie)));
    }

    void notify_data_error(uint32_t cookie, ListError::Code error) const final override
    {
        GVariantBuilder b;
        g_variant_builder_init(&b, reinterpret_cast<const GVariantType *>("a(uy)"));
        g_variant_builder_add(&b, "(uy)", cookie, error);
        tdbus_lists_navigation_emit_data_error(DBusNavlists::get_navigation_iface(),
                                               g_variant_builder_end(&b));
    }

  protected:
    void do_cancel(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock) final override
    {
        if(this->begin_cancel_request())
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

/*!
 * Handler for de.tahifi.Lists.Navigation.GetListContexts().
 */
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
  private:
    static const std::string NAME;
    static constexpr const char *const DBUS_RETURN_TYPE_STRING = "a(sy)";
    static constexpr const char *const DBUS_ELEMENT_TYPE_STRING = "(sy)";

    const ID::List list_id_;
    const ID::Item first_item_id_;
    const size_t count_;

  public:
    GetRange(GetRange &&) = delete;
    GetRange &operator=(GetRange &&) = delete;

    explicit GetRange(ListTreeIface &listtree, ID::List list_id,
                      ID::Item first_item_id, size_t count):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        first_item_id_(first_item_id),
        count_(count)
    {
        msg_log_assert(list_id_.is_valid());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_range(
            object, invocation, cookie, error, 0,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr));
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_range_by_cookie(
            object, invocation, error, 0,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr));
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
            items_in_range = g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr);
        }

        promise_.set_value(
            std::make_tuple(error, error.failed() ? ID::Item() : first_item_id_,
                            GVariantWrapper(items_in_range)));
        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetRange::NAME("GetRange");

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRange().
 */
gboolean DBusNavlists::get_range(tdbuslistsNavigation *object,
                                 GDBusMethodInvocation *invocation,
                                 guint list_id, guint first_item_id,
                                 guint count, IfaceData *data)
{
    enter_handler(invocation);

    const ID::List id(list_id);

    if(!data->listtree_.use_list(id, false))
    {
        GetRange::fast_path_failure(object, invocation, 0, ListError::INVALID_ID);
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetRange>(
        object, invocation,
        data->listtree_.q_navlists_get_range_,
        std::make_shared<GetRange>(data->listtree_, id,
                                   ID::Item(first_item_id), count),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_range(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                GVariantWrapper::move(std::get<2>(result)));
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRangeByCookie().
 */
gboolean DBusNavlists::get_range_by_cookie(tdbuslistsNavigation *object,
                                           GDBusMethodInvocation *invocation,
                                           guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetRange>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_range_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                GVariantWrapper::move(std::get<2>(result)));
        });

    return TRUE;
}

class GetRangeWithMetaData: public NavListsWork<std::tuple<ListError, ID::Item, GVariantWrapper>>
{
  private:
    static const std::string NAME;
    static constexpr const char *const DBUS_RETURN_TYPE_STRING = "a(sssyy)";
    static constexpr const char *const DBUS_ELEMENT_TYPE_STRING = "(sssyy)";

    const ID::List list_id_;
    const ID::Item first_item_id_;
    const size_t count_;

  public:
    GetRangeWithMetaData(GetRangeWithMetaData &&) = delete;
    GetRangeWithMetaData &operator=(GetRangeWithMetaData &&) = delete;

    explicit GetRangeWithMetaData(ListTreeIface &listtree, ID::List list_id,
                      ID::Item first_item_id, size_t count):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        first_item_id_(first_item_id),
        count_(count)
    {
        msg_log_assert(list_id_.is_valid());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_range_with_meta_data(
            object, invocation, cookie, error, 0,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr));
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_range_with_meta_data_by_cookie(
            object, invocation, error, 0,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr));
    }

  protected:
    bool do_run() final override
    {
        listtree_.use_list(list_id_, false);

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE(DBUS_RETURN_TYPE_STRING));

        const ListError error =
            listtree_.for_each(list_id_, first_item_id_, count_,
                [&builder] (const ListTreeIface::ForEachItemDataDetailed &item_data)
                {
                    msg_info("for_each(): \"%s\"/\"%s\"/\"%s\", primary %u, %s dir",
                             item_data.artist_.c_str(),
                             item_data.album_.c_str(),
                             item_data.title_.c_str(),
                             item_data.primary_string_index_,
                             item_data.kind_.is_directory() ? "is" : "no");
                    g_variant_builder_add(&builder, DBUS_ELEMENT_TYPE_STRING,
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
            g_variant_unref(items_in_range);
            items_in_range = g_variant_new(DBUS_ELEMENT_TYPE_STRING, nullptr);
        }

        promise_.set_value(
            std::make_tuple(error, error.failed() ? ID::Item() : first_item_id_,
                            GVariantWrapper(items_in_range)));
        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetRangeWithMetaData::NAME = "GetRangeWithMetaData";

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRangeWithMetaData().
 */
gboolean DBusNavlists::get_range_with_meta_data(tdbuslistsNavigation *object,
                                                GDBusMethodInvocation *invocation,
                                                guint list_id,
                                                guint first_item_id,
                                                guint count, IfaceData *data)
{
    enter_handler(invocation);

    const ID::List id(list_id);

    if(!data->listtree_.use_list(id, false))
    {
        GetRangeWithMetaData::fast_path_failure(object, invocation,
                                                0, ListError::INVALID_ID);
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetRangeWithMetaData>(
        object, invocation,
        data->listtree_.q_navlists_get_range_,
        std::make_shared<GetRangeWithMetaData>(data->listtree_, id,
                                               ID::Item(first_item_id), count),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_range_with_meta_data(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                GVariantWrapper::move(std::get<2>(result)));
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRangeWithMetaDataByCookie().
 */
gboolean DBusNavlists::get_range_with_meta_data_by_cookie(
            tdbuslistsNavigation *object, GDBusMethodInvocation *invocation,
            guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetRangeWithMetaData>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_range_with_meta_data_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                GVariantWrapper::move(std::get<2>(result)));
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.CheckRange().
 */
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
    static const std::string NAME;
    const ID::List list_id_;
    const ID::Item item_id_;

  public:
    GetListID(GetListID &&) = delete;
    GetListID &operator=(GetListID &&) = delete;

    explicit GetListID(ListTreeIface &listtree,
                       ID::List list_id, ID::Item item_id):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        item_id_(item_id)
    {}

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_list_id(object, invocation, cookie,
                                                    error, 0, "", FALSE);
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_list_id_by_cookie(
            object, invocation, error, 0, "", FALSE);
    }

  protected:
    bool do_run() final override
    {
        if(listtree_.use_list(list_id_, false))
        {
            ListError error;
            const auto child_id = listtree_.enter_child(list_id_, item_id_, error);

            if(child_id.is_valid())
                promise_.set_value(std::make_tuple(
                    error, child_id,
                    listtree_.get_child_list_title(list_id_, item_id_)));
            else
                promise_.set_value(std::make_tuple(
                    error, child_id, I18n::String(false)));

            put_error(error);

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

const std::string GetListID::NAME = "GetListID";

/*!
 * Handler for de.tahifi.Lists.Navigation.GetListId().
 */
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

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetListID>(
        object, invocation,
        data->listtree_.q_navlists_get_list_id_,
        std::make_shared<GetListID>(data->listtree_,
                                    list_id == 0 ? ID::List() : ID::List(list_id),
                                    list_id == 0 ? ID::Item() : ID::Item(item_id)),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_list_id(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                std::get<2>(result).get_text().c_str(),
                std::get<2>(result).is_translatable());
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetListIdByCookie().
 */
gboolean DBusNavlists::get_list_id_by_cookie(tdbuslistsNavigation *object,
                                             GDBusMethodInvocation *invocation,
                                             guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetListID>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_list_id_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                std::get<2>(result).get_text().c_str(),
                std::get<2>(result).is_translatable());
        });

    return TRUE;
}

class GetParamListID: public NavListsWork<std::tuple<ListError, ID::List, I18n::String>>
{
  private:
    static const std::string NAME;
    const ID::List list_id_;
    const ID::Item item_id_;
    const std::string parameter_;

  public:
    GetParamListID(GetParamListID &&) = delete;
    GetParamListID &operator=(GetParamListID &&) = delete;

    explicit GetParamListID(ListTreeIface &listtree,
                            ID::List list_id, ID::Item item_id,
                            std::string &&parameter):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        item_id_(item_id),
        parameter_(std::move(parameter))
    {}

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_parameterized_list_id(
            object, invocation, cookie, error, 0, "", FALSE);
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_parameterized_list_id_by_cookie(
            object, invocation, error, 0, "", FALSE);
    }

  protected:
    bool do_run() final override
    {
        if(!listtree_.use_list(list_id_, false))
        {
            put_error(ListError(ListError::INVALID_ID));
            promise_.set_value(std::make_tuple(
                ListError(ListError::INVALID_ID), ID::List(), I18n::String(false)));
            return true;
        }

        ListError error;
        const auto child_id =
            listtree_.enter_child_with_parameters(list_id_, item_id_,
                                                  parameter_.c_str(), error);

        if(child_id.is_valid())
            promise_.set_value(std::make_tuple(
                error, child_id,
                listtree_.get_child_list_title(list_id_, item_id_)));
        else
            promise_.set_value(std::make_tuple(error, child_id, I18n::String(false)));

        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetParamListID::NAME = "GetParamListID";

/*!
 * Handler for de.tahifi.Lists.Navigation.GetParameterizedListId().
 */
gboolean DBusNavlists::get_parameterized_list_id(tdbuslistsNavigation *object,
                                                 GDBusMethodInvocation *invocation,
                                                 guint list_id, guint item_id,
                                                 const gchar *parameter,
                                                 IfaceData *data)
{
    enter_handler(invocation);

    if(list_id == 0)
    {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Root lists are not parameterized");
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetParamListID>(
        object, invocation,
        data->listtree_.q_navlists_get_list_id_,
        std::make_shared<GetParamListID>(data->listtree_, ID::List(list_id),
                                         ID::Item(item_id), parameter),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_parameterized_list_id(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                std::get<2>(result).get_text().c_str(),
                std::get<2>(result).is_translatable());
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetParameterizedListIdByCookie().
 */
gboolean DBusNavlists::get_parameterized_list_id_by_cookie(
            tdbuslistsNavigation *object, GDBusMethodInvocation *invocation,
            guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetParamListID>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_parameterized_list_id_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                std::get<1>(result).get_raw_id(),
                std::get<2>(result).get_text().c_str(),
                std::get<2>(result).is_translatable());
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetParentLink().
 */
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

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRootLinkToContext().
 */
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


class GetURIs: public NavListsWork<std::tuple<ListError, std::vector<Url::String>,
                                              ListItemKey>>
{
  private:
    static const std::string NAME;
    const ID::List list_id_;
    const ID::Item item_id_;

  public:
    GetURIs(GetURIs &&) = delete;
    GetURIs &operator=(GetURIs &&) = delete;

    explicit GetURIs(ListTreeIface &listtree, ID::List list_id, ID::Item item_id):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        item_id_(item_id)
    {
        msg_log_assert(list_id_.is_valid());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        static const char *const empty_list[] = { nullptr };
        tdbus_lists_navigation_complete_get_uris(
            object, invocation, cookie, error, empty_list,
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, nullptr,
                                      0, sizeof(unsigned char)));
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        static const char *const empty_list[] = { nullptr };
        tdbus_lists_navigation_complete_get_uris_by_cookie(
            object, invocation, error, empty_list,
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, nullptr,
                                      0, sizeof(unsigned char)));
    }

  protected:
    bool do_run() final override
    {
        std::vector<Url::String> uris;
        ListItemKey item_key;
        ListError error =
            listtree_.get_uris_for_item(list_id_, item_id_, uris, item_key);

        promise_.set_value(
            std::make_tuple(error, std::move(uris), std::move(item_key)));
        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetURIs::NAME = "GetURIs";

static std::vector<const gchar *>
uri_list_to_c_array(const std::vector<Url::String> &uris, const ListError &error)
{
    std::vector<const gchar *> c_array;

    if(!error.failed())
        std::transform(uris.begin(), uris.end(),
            std::back_inserter(c_array),
            [] (const auto &uri) { return uri.get_cleartext().c_str(); });

    c_array.push_back(nullptr);

    return c_array;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetURIs().
 */
gboolean DBusNavlists::get_uris(tdbuslistsNavigation *object,
                                GDBusMethodInvocation *invocation,
                                guint list_id, guint item_id, IfaceData *data)
{
    enter_handler(invocation);

    const ID::List id(list_id);

    if(!data->listtree_.use_list(id, true))
    {
        GetURIs::fast_path_failure(object, invocation, 0, ListError::INVALID_ID);
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetURIs>(
        object, invocation,
        data->listtree_.q_navlists_get_uris_,
        std::make_shared<GetURIs>(data->listtree_, id, ID::Item(item_id)),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            const ListError &error(std::get<0>(result));
            const auto list_of_uris_for_dbus =
                uri_list_to_c_array(std::get<1>(result), error);

            tdbus_lists_navigation_complete_get_uris(
                obj, inv, 0, error.get_raw_code(),
                list_of_uris_for_dbus.data(), hash_to_variant(std::get<2>(result)));
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetURIsByCookie().
 */
gboolean DBusNavlists::get_uris_by_cookie(tdbuslistsNavigation *object,
                                          GDBusMethodInvocation *invocation,
                                          guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetURIs>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            const ListError &error(std::get<0>(result));
            const auto list_of_uris_for_dbus =
                uri_list_to_c_array(std::get<1>(result), error);

            tdbus_lists_navigation_complete_get_uris_by_cookie(
                obj, inv, error.get_raw_code(),
                list_of_uris_for_dbus.data(), hash_to_variant(std::get<2>(result)));
        });

    return TRUE;
}

class GetRankedStreamLinks:
    public NavListsWork<std::tuple<ListError, GVariantWrapper, ListItemKey>>
{
  private:
    static const std::string NAME;
    static constexpr const char *const DBUS_RETURN_TYPE_STRING = "a(uus)";
    static constexpr const char *const DBUS_ELEMENT_TYPE_STRING = "(uus)";

    const ID::List list_id_;
    const ID::Item item_id_;

  public:
    GetRankedStreamLinks(GetRankedStreamLinks &&) = delete;
    GetRankedStreamLinks &operator=(GetRankedStreamLinks &&) = delete;

    explicit GetRankedStreamLinks(ListTreeIface &listtree,
                                  ID::List list_id, ID::Item item_id):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        item_id_(item_id)
    {
        msg_log_assert(list_id_.is_valid());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_ranked_stream_links(
            object, invocation, cookie, error,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr),
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, nullptr,
                                      0, sizeof(unsigned char)));
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_ranked_stream_links_by_cookie(
            object, invocation, error,
            g_variant_new(DBUS_RETURN_TYPE_STRING, nullptr),
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, nullptr,
                                      0, sizeof(unsigned char)));
    }

  protected:
    bool do_run() final override
    {
        std::vector<Url::RankedStreamLinks> ranked_links;
        ListItemKey item_key;
        ListError error =
            listtree_.get_ranked_links_for_item(list_id_, item_id_,
                                                ranked_links, item_key);

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE(DBUS_RETURN_TYPE_STRING));

        if(!error.failed())
            for(const auto &l : ranked_links)
                g_variant_builder_add(&builder, DBUS_ELEMENT_TYPE_STRING,
                                      l.get_rank(), l.get_bitrate(),
                                      l.get_stream_link().url_.get_cleartext().c_str());

        GVariantWrapper links(g_variant_builder_end(&builder));

        promise_.set_value(
            std::make_tuple(error, std::move(links), std::move(item_key)));
        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetRankedStreamLinks::NAME = "GetRankedStreamLinks";

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRankedStreamLinks().
 */
gboolean DBusNavlists::get_ranked_stream_links(tdbuslistsNavigation *object,
                                               GDBusMethodInvocation *invocation,
                                               guint list_id, guint item_id,
                                               IfaceData *data)
{
    enter_handler(invocation);

    const ID::List id(list_id);

    if(!data->listtree_.use_list(id, true))
    {
        GetRankedStreamLinks::fast_path_failure(object, invocation,
                                                0, ListError::INVALID_ID);
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetRankedStreamLinks>(
        object, invocation,
        data->listtree_.q_navlists_get_uris_,
        std::make_shared<GetRankedStreamLinks>(data->listtree_, id, ID::Item(item_id)),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_ranked_stream_links(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                GVariantWrapper::move(std::get<1>(result)),
                hash_to_variant(std::get<2>(result)));
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetRankedStreamLinksByCookie().
 */
gboolean
DBusNavlists::get_ranked_stream_links_by_cookie(tdbuslistsNavigation *object,
                                                GDBusMethodInvocation *invocation,
                                                guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetRankedStreamLinks>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            tdbus_lists_navigation_complete_get_ranked_stream_links_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                GVariantWrapper::move(std::get<1>(result)),
                hash_to_variant(std::get<2>(result)));
        });

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

/*!
 * Handler for de.tahifi.Lists.Navigation.GetLocationKey().
 */
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

        std::unique_ptr<StrBoUrl::Location> location = error.failed()
            ? nullptr
            : data->listtree_.get_location_key(id, StrBoUrl::ObjectIndex(item_id),
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

class GetLocationTrace:
    public NavListsWork<std::tuple<ListError, std::unique_ptr<StrBoUrl::Location>>>
{
  private:
    static const std::string NAME;
    const ID::List list_id_;
    const StrBoUrl::ObjectIndex item_id_;
    const ID::List ref_list_id_;
    const StrBoUrl::ObjectIndex ref_item_id_;

  public:
    GetLocationTrace(GetLocationTrace &&) = delete;
    GetLocationTrace &operator=(GetLocationTrace &&) = delete;

    explicit GetLocationTrace(ListTreeIface &listtree,
                              ID::List list_id, StrBoUrl::ObjectIndex item_id,
                              ID::List ref_list_id, StrBoUrl::ObjectIndex ref_item_id):
        NavListsWork(NAME, listtree),
        list_id_(list_id),
        item_id_(item_id),
        ref_list_id_(ref_list_id),
        ref_item_id_(ref_item_id)
    {
        msg_log_assert(list_id_.is_valid());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_location_trace(
            object, invocation, cookie, error, "");
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_get_location_trace_by_cookie(
            object, invocation, error, "");
    }

  protected:
    bool do_run() final override
    {
        ListError error;
        auto location =
            listtree_.get_location_trace(list_id_, item_id_,
                                         ref_list_id_, ref_item_id_, error);

        promise_.set_value(std::make_tuple(error, std::move(location)));
        put_error(error);

        return error != ListError::INTERRUPTED;
    }
};

const std::string GetLocationTrace::NAME = "GetLocationTrace";

/*!
 * Handler for de.tahifi.Lists.Navigation.GetLocationTrace().
 */
gboolean DBusNavlists::get_location_trace(tdbuslistsNavigation *object,
                                          GDBusMethodInvocation *invocation,
                                          guint list_id, guint item_id,
                                          guint ref_list_id, guint ref_item_id,
                                          IfaceData *data)
{
    enter_handler(invocation);

    const auto obj_list_id = ID::List(list_id);
    ListError error;

    if(!obj_list_id.is_valid())
        error = ListError::INVALID_ID;
    else
    {
        if(item_id == 0 ||
           (ref_list_id != 0 && ref_item_id == 0) ||
           obj_list_id == ID::List(ref_list_id))
            error = ListError::NOT_SUPPORTED;
        else if(ref_list_id == 0 && ref_item_id != 0)
            error = ListError::INVALID_ID;
    }

    if(error.failed())
    {
        GetLocationTrace::fast_path_failure(object, invocation, 0, error.get());
        return TRUE;
    }

    DBusAsync::try_fast_path<tdbuslistsNavigation, GetLocationTrace>(
        object, invocation,
        data->listtree_.q_navlists_realize_location_,
        std::make_shared<GetLocationTrace>(data->listtree_,
                                           obj_list_id, StrBoUrl::ObjectIndex(item_id),
                                           ID::List(ref_list_id),
                                           StrBoUrl::ObjectIndex(ref_item_id)),
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            auto p = std::move(std::get<1>(result));
            tdbus_lists_navigation_complete_get_location_trace(
                obj, inv, 0, std::get<0>(result).get_raw_code(),
                p != nullptr ? p->str().c_str() : "");
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.GetLocationTraceByCookie().
 */
gboolean DBusNavlists::get_location_trace_by_cookie(
            tdbuslistsNavigation *object, GDBusMethodInvocation *invocation,
            guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, GetLocationTrace>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            auto p = std::move(std::get<1>(result));
            tdbus_lists_navigation_complete_get_location_trace_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                p != nullptr ? p->str().c_str() : "");
        });

    return TRUE;
}

class RealizeLocation: public NavListsWork<std::tuple<ListError, ListTreeIface::RealizeURLResult>>
{
  private:
    static const std::string NAME;
    const std::string url_;

  public:
    RealizeLocation(RealizeLocation &&) = delete;
    RealizeLocation &operator=(RealizeLocation &&) = delete;

    explicit RealizeLocation(ListTreeIface &listtree, std::string &&url):
        NavListsWork(NAME, listtree),
        url_(std::move(url))
    {
        msg_log_assert(!url_.empty());
    }

    static void fast_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  uint32_t cookie, ListError::Code error)
    {
        tdbus_lists_navigation_complete_realize_location(object, invocation,
                                                         0, error);
    }

    static void slow_path_failure(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  ListError::Code error)
    {
        tdbus_lists_navigation_complete_realize_location_by_cookie(
            object, invocation, error, 0, 0, 0, 0, 0, 0, "", FALSE);
    }

  protected:
    bool do_run() final override
    {
        ListTreeIface::RealizeURLResult result;
        const auto error = listtree_.realize_strbo_url(url_, result);
        promise_.set_value(std::make_tuple(error, std::move(result)));
        put_error(error);
        return error != ListError::INTERRUPTED;
    }
};

const std::string RealizeLocation::NAME = "RealizeLocation";

/*!
 * Handler for de.tahifi.Lists.Navigation.RealizeLocation().
 */
gboolean DBusNavlists::realize_location(tdbuslistsNavigation *object,
                                        GDBusMethodInvocation *invocation,
                                        const gchar *location_url,
                                        IfaceData *data)
{
    enter_handler(invocation);

    if(location_url[0] == '\0')
    {
        RealizeLocation::fast_path_failure(object, invocation, 0,
                                           ListError::INVALID_STRBO_URL);
        return TRUE;
    }

    if(!data->listtree_.can_handle_strbo_url(location_url))
    {
        RealizeLocation::fast_path_failure(object, invocation, 0,
                                           ListError::NOT_SUPPORTED);
        return TRUE;
    }

    auto work = std::make_shared<RealizeLocation>(data->listtree_, location_url);
    const uint32_t cookie =
        DBusAsync::get_cookie_jar_singleton().pick_cookie_for_work(
            work, DBusAsync::CookieJar::DataAvailableNotificationMode::ALWAYS);

    data->listtree_.q_navlists_realize_location_.add_work(
        std::move(work),
        [object, invocation, cookie] (bool is_async, bool is_sync_done)
        {
            if(is_async)
                tdbus_lists_navigation_complete_realize_location(
                    object, invocation, cookie, ListError::BUSY);
            else if(is_sync_done)
                tdbus_lists_navigation_complete_realize_location(
                    object, invocation, 0, ListError::OK);
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.RealizeLocationByCookie().
 */
gboolean DBusNavlists::realize_location_by_cookie(tdbuslistsNavigation *object,
                                                  GDBusMethodInvocation *invocation,
                                                  guint cookie, IfaceData *data)
{
    enter_handler(invocation);

    DBusAsync::finish_slow_path<tdbuslistsNavigation, RealizeLocation>(
        object, invocation, cookie,
        [] (tdbuslistsNavigation *obj, GDBusMethodInvocation *inv, auto &&result)
        {
            const auto &url_result(std::get<1>(result));
            tdbus_lists_navigation_complete_realize_location_by_cookie(
                obj, inv, std::get<0>(result).get_raw_code(),
                url_result.list_id.get_raw_id(), url_result.item_id.get_raw_id(),
                url_result.ref_list_id.get_raw_id(),
                url_result.ref_item_id.get_raw_id(),
                url_result.distance, url_result.trace_length,
                url_result.list_title.get_text().c_str(),
                url_result.list_title.is_translatable());
        });

    return TRUE;
}

/*!
 * Handler for de.tahifi.Lists.Navigation.DataAbort().
 */
gboolean DBusNavlists::data_abort(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  GVariant *cookies, IfaceData *data)
{
    enter_handler(invocation);

    GVariantIter iter;
    g_variant_iter_init(&iter, cookies);
    guint cookie;
    gboolean keep_around;

    while(g_variant_iter_loop(&iter,"(ub)", &cookie, &keep_around))
    {
        if(!keep_around)
            DBusAsync::get_cookie_jar_singleton().cookie_not_wanted(cookie);
        else
            MSG_NOT_IMPLEMENTED();
    }

    tdbus_lists_navigation_complete_data_abort(object, invocation);

    return TRUE;
}
