/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_LISTS_HANDLERS_H
#define DBUS_LISTS_HANDLERS_H

#include <gio/gio.h>

#include "lists_dbus.h"

/*!
 * \addtogroup dbus_handlers_navlists Handlers for de.tahifi.Lists.Navigation interface.
 * \ingroup dbus_handlers
 */
/*!@{*/

#ifdef __cplusplus
extern "C" {
#endif

struct DBusNavlistsIfaceData;

gboolean dbusmethod_navlists_get_list_contexts(tdbuslistsNavigation *object,
                                               GDBusMethodInvocation *invocation,
                                               struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_range(tdbuslistsNavigation *object,
                                       GDBusMethodInvocation *invocation,
                                       guint list_id, guint first_item_id,
                                       guint count,
                                       struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_range_with_meta_data(tdbuslistsNavigation *object,
                                                      GDBusMethodInvocation *invocation,
                                                      guint list_id,
                                                      guint first_item_id,
                                                      guint count,
                                                      struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_check_range(tdbuslistsNavigation *object,
                                         GDBusMethodInvocation *invocation,
                                         guint list_id, guint first_item_id,
                                         guint count,
                                         struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_list_id(tdbuslistsNavigation *object,
                                         GDBusMethodInvocation *invocation,
                                         guint list_id, guint item_id,
                                         struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_parameterized_list_id(tdbuslistsNavigation *object,
                                                       GDBusMethodInvocation *invocation,
                                                       guint list_id, guint item_id,
                                                       const gchar *parameter,
                                                       struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_parent_link(tdbuslistsNavigation *object,
                                             GDBusMethodInvocation *invocation,
                                             guint list_id,
                                             struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_root_link_to_context(tdbuslistsNavigation *object,
                                                      GDBusMethodInvocation *invocation,
                                                      const gchar *context,
                                                      struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_uris(tdbuslistsNavigation *object,
                                      GDBusMethodInvocation *invocation,
                                      guint list_id, guint item_id,
                                      struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_ranked_stream_links(tdbuslistsNavigation *object,
                                                     GDBusMethodInvocation *invocation,
                                                     guint list_id, guint item_id,
                                                     struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_discard_list(tdbuslistsNavigation *object,
                                          GDBusMethodInvocation *invocation,
                                          guint list_id,
                                          struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_keep_alive(tdbuslistsNavigation *object,
                                        GDBusMethodInvocation *invocation,
                                        GVariant *list_ids,
                                        struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_force_in_cache(tdbuslistsNavigation *object,
                                            GDBusMethodInvocation *invocation,
                                            guint list_id, gboolean force,
                                            struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_location_key(tdbuslistsNavigation *object,
                                              GDBusMethodInvocation *invocation,
                                              guint list_id, guint item_id,
                                              gboolean as_reference_key,
                                              struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_get_location_trace(tdbuslistsNavigation *object,
                                                GDBusMethodInvocation *invocation,
                                                guint list_id, guint item_id,
                                                guint ref_list_id, guint ref_item_id,
                                                struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_realize_location(tdbuslistsNavigation *object,
                                              GDBusMethodInvocation *invocation,
                                              const gchar *location_url,
                                              struct DBusNavlistsIfaceData *data);
gboolean dbusmethod_navlists_abort_realize_location(tdbuslistsNavigation *object,
                                                    GDBusMethodInvocation *invocation,
                                                    guint cookie,
                                                    struct DBusNavlistsIfaceData *data);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* !DBUS_LISTS_HANDLERS_H */
