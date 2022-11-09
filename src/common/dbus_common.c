/*
 * Copyright (C) 2015--2017, 2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#include <string.h>
#include <errno.h>

#include "dbus_common.h"
#include "messages.h"

static size_t number_of_registered_submodules;
static struct dbus_register_submodule_t registered_submodules[6];

void dbus_common_register_submodule(const struct dbus_register_submodule_t *submodule)
{
    msg_log_assert(submodule != NULL);
    msg_log_assert(number_of_registered_submodules < sizeof(registered_submodules) / sizeof(registered_submodules[0]));

    /* use memcpy() instead of simple assignment to avoid warning due to
     * assignment to const members */
    memcpy(&registered_submodules[number_of_registered_submodules++],
           submodule, sizeof(*submodule));
}

struct dbus_data
{
    guint owner_id;
    int name_acquired;
};

static struct dbus_data dbus_data_system_bus;
static struct dbus_data dbus_data_session_bus;

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    const bool is_session_bus = (user_data == &dbus_data_session_bus);

    for(size_t i = 0; i < number_of_registered_submodules; ++i)
    {
        const struct dbus_register_submodule_t *const submodule =
            &registered_submodules[i];

        if((!!submodule->connect_to_session_bus) == is_session_bus &&
           submodule->bus_acquired != NULL)
            submodule->bus_acquired(connection, name, is_session_bus,
                                    submodule->user_data);
    }
}

static void name_acquired(GDBusConnection *connection,
                          const gchar *name, gpointer user_data)
{
    const bool is_session_bus = (user_data == &dbus_data_session_bus);
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" acquired (%s bus)",
             name, is_session_bus ? "session" : "system");
    data->name_acquired = 1;

    for(size_t i = 0; i < number_of_registered_submodules; ++i)
    {
        const struct dbus_register_submodule_t *const submodule =
            &registered_submodules[i];

        if((!!submodule->connect_to_session_bus) == is_session_bus &&
           submodule->name_acquired != NULL)
            submodule->name_acquired(connection, name, is_session_bus,
                                     submodule->user_data);
    }
}

static void destroy_notification(gpointer user_data)
{
    const bool is_session_bus = (user_data == &dbus_data_session_bus);

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
              "%s bus connection destroyed.", is_session_bus ? "Session" : "System");

    for(size_t i = 0; i < number_of_registered_submodules; ++i)
    {
        const struct dbus_register_submodule_t *const submodule =
            &registered_submodules[i];

        if((!!submodule->connect_to_session_bus) == is_session_bus &&
           submodule->destroy_notification != NULL)
            submodule->destroy_notification(submodule->connect_to_session_bus,
                                            submodule->user_data);
    }
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    const bool is_session_bus = (user_data == &dbus_data_session_bus);
    struct dbus_data *data = user_data;

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
              "D-Bus name \"%s\" lost (%s bus)",
             name, is_session_bus ? "session" : "system");
    data->name_acquired = -1;
}

int dbus_common_setup(GMainLoop *loop, const char *bus_name)
{
    memset(&dbus_data_system_bus, 0, sizeof(dbus_data_system_bus));
    memset(&dbus_data_session_bus, 0, sizeof(dbus_data_session_bus));

    for(size_t i = 0; i < number_of_registered_submodules; ++i)
    {
        const struct dbus_register_submodule_t *const submodule =
            &registered_submodules[i];
        struct dbus_data *const dbus_data = (submodule->connect_to_session_bus
                                             ? &dbus_data_session_bus
                                             : &dbus_data_system_bus);
        const GBusType bus_type = (submodule->connect_to_session_bus
                                   ? G_BUS_TYPE_SESSION
                                   : G_BUS_TYPE_SYSTEM);

        if(dbus_data->owner_id == 0)
            dbus_data->owner_id =
                g_bus_own_name(bus_type, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
                               bus_acquired, name_acquired, name_lost, dbus_data,
                               destroy_notification);
    }

    if(dbus_data_system_bus.owner_id == 0 &&
       dbus_data_session_bus.owner_id == 0)
    {
        msg_info("Not connecting to D-Bus, no submodules have registered");
        return 0;
    }

    while((dbus_data_system_bus.owner_id == 0 || dbus_data_system_bus.name_acquired == 0) &&
          (dbus_data_session_bus.owner_id == 0 || dbus_data_session_bus.name_acquired == 0))
    {
        /* do whatever has to be done behind the scenes until one of the
         * guaranteed callbacks gets called for each bus */
        g_main_context_iteration(NULL, TRUE);
    }

    bool failed = false;

    if(dbus_data_system_bus.owner_id > 0 && dbus_data_system_bus.name_acquired < 0)
    {
        msg_error(EPIPE, LOG_EMERG, "Failed acquiring D-Bus name on system bus");
        failed = true;
    }

    if(dbus_data_session_bus.owner_id > 0 && dbus_data_session_bus.name_acquired < 0)
    {
        msg_error(EPIPE, LOG_EMERG, "Failed acquiring D-Bus name on session bus");
        failed = true;
    }

    if(failed)
        return -1;

    g_main_loop_ref(loop);

    return 0;
}

void dbus_common_shutdown(GMainLoop *loop)
{
    if(loop == NULL)
        return;

    if(dbus_data_system_bus.owner_id > 0)
        g_bus_unown_name(dbus_data_system_bus.owner_id);

    if(dbus_data_session_bus.owner_id)
        g_bus_unown_name(dbus_data_session_bus.owner_id);

    g_main_loop_unref(loop);

    for(size_t i = 0; i < number_of_registered_submodules; ++i)
    {
        const struct dbus_register_submodule_t *const submodule =
            &registered_submodules[i];

        submodule->shutdown(submodule->connect_to_session_bus,
                            submodule->user_data);
    }
}

int dbus_common_try_export_iface(GDBusConnection *connection,
                                 GDBusInterfaceSkeleton *iface,
                                 const char *dbus_object_path)
{
    GError *error = NULL;

    g_dbus_interface_skeleton_export(iface, connection, dbus_object_path, &error);

    if(!error)
        return 0;

    msg_error(0, LOG_EMERG, "%s", error->message);
    g_error_free(error);

    return -1;
}

void dbus_common_unknown_signal(const char *iface_name, const char *signal_name,
                                const char *sender_name)
{
    msg_error(ENOSYS, LOG_NOTICE, "Got unknown signal %s.%s from %s",
              iface_name, signal_name, sender_name);
}
