/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_LISTS_HANDLERS_HH
#define DBUS_LISTS_HANDLERS_HH

#include "listtree.hh"
#include "de_tahifi_lists.h"

/*!
 * \addtogroup dbus
 */
/*!@{*/

namespace DBusNavlists
{

/*!
 * Structure passed to D-Bus method handlers concerning list navigation.
 */
struct IfaceData
{
    ListTreeIface &listtree_;

    explicit IfaceData(ListTreeIface &lt):
        listtree_(lt)
    {}
};

void dbus_setup(bool connect_to_session_bus, const char *dbus_object_path,
                IfaceData *iface_data);

/*!
 * \addtogroup dbus_handlers_navlists Handlers for de.tahifi.Lists.Navigation interface.
 * \ingroup dbus_handlers
 */
/*!@{*/

extern "C" {

gboolean get_list_contexts(tdbuslistsNavigation *object,
                           GDBusMethodInvocation *invocation, IfaceData *data);
gboolean get_range(tdbuslistsNavigation *object,
                   GDBusMethodInvocation *invocation, guint list_id,
                   guint first_item_id, guint count, IfaceData *data);
gboolean get_range_with_meta_data(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  guint list_id, guint first_item_id,
                                  guint count, IfaceData *data);
gboolean check_range(tdbuslistsNavigation *object,
                     GDBusMethodInvocation *invocation,
                     guint list_id, guint first_item_id, guint count,
                     IfaceData *data);
gboolean get_list_id(tdbuslistsNavigation *object,
                     GDBusMethodInvocation *invocation,
                     guint list_id, guint item_id, IfaceData *data);
gboolean get_parameterized_list_id(tdbuslistsNavigation *object,
                                   GDBusMethodInvocation *invocation,
                                   guint list_id, guint item_id,
                                   const gchar *parameter, IfaceData *data);
gboolean get_parent_link(tdbuslistsNavigation *object,
                         GDBusMethodInvocation *invocation,
                         guint list_id, IfaceData *data);
gboolean get_root_link_to_context(tdbuslistsNavigation *object,
                                  GDBusMethodInvocation *invocation,
                                  const gchar *context, IfaceData *data);
gboolean get_uris(tdbuslistsNavigation *object,
                  GDBusMethodInvocation *invocation,
                  guint list_id, guint item_id, IfaceData *data);
gboolean get_ranked_stream_links(tdbuslistsNavigation *object,
                                 GDBusMethodInvocation *invocation,
                                 guint list_id, guint item_id, IfaceData *data);
gboolean discard_list(tdbuslistsNavigation *object,
                      GDBusMethodInvocation *invocation,
                      guint list_id, IfaceData *data);
gboolean keep_alive(tdbuslistsNavigation *object,
                    GDBusMethodInvocation *invocation,
                    GVariant *list_ids, IfaceData *data);
gboolean force_in_cache(tdbuslistsNavigation *object,
                        GDBusMethodInvocation *invocation,
                        guint list_id, gboolean force, IfaceData *data);
gboolean get_location_key(tdbuslistsNavigation *object,
                          GDBusMethodInvocation *invocation,
                          guint list_id, guint item_id, gboolean as_reference_key,
                          IfaceData *data);
gboolean get_location_trace(tdbuslistsNavigation *object,
                            GDBusMethodInvocation *invocation,
                            guint list_id, guint item_id,
                            guint ref_list_id, guint ref_item_id,
                            IfaceData *data);
gboolean realize_location(tdbuslistsNavigation *object,
                          GDBusMethodInvocation *invocation,
                          const gchar *location_url, IfaceData *data);
gboolean realize_location_by_cookie(tdbuslistsNavigation *object,
                                    GDBusMethodInvocation *invocation,
                                    guint cookie, IfaceData *data);
gboolean data_abort(tdbuslistsNavigation *object,
                    GDBusMethodInvocation *invocation,
                    GVariant *cookies, IfaceData *data);

}

/*!@}*/

}

/*!@}*/

#endif /* !DBUS_LISTS_HANDLERS_HH */
