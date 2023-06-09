/*
 * Copyright (C) 2015, 2016, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_usb_iface.hh"
#include "dbus_usb_iface_deep.h"
#include "dbus_common.h"
#include "gerrorwrapper.hh"

struct DBusUSBData
{
    const char *dbus_object_path;

    tdbusMounTA *mounta_iface;
    DBusMounTA::SignalData *signal_data;
};

static void connect_dbus_signals(GDBusConnection *connection,
                                 const gchar *name, bool is_session_bus,
                                 gpointer user_data)
{
    auto *data = static_cast<DBusUSBData *>(user_data);

    GErrorWrapper error;

    data->mounta_iface =
        tdbus_moun_ta_proxy_new_sync(connection, G_DBUS_PROXY_FLAGS_NONE,
                                     "de.tahifi.MounTA", "/de/tahifi/MounTA",
                                     NULL, error.await());
    if(!error.log_failure("Create MounTA proxy"))
        g_signal_connect(data->mounta_iface, "g-signal",
                         G_CALLBACK(DBusMounTA::signal_handler), data->signal_data);
}

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    auto *data = static_cast<DBusUSBData *>(user_data);

    g_object_unref(data->mounta_iface);
}

static DBusUSBData dbus_usb_data;

void DBusUSB::dbus_setup(bool connect_to_session_bus, const char *dbus_object_path,
                         DBusMounTA::SignalData *signal_data)
{
    dbus_usb_data.dbus_object_path = dbus_object_path;
    dbus_usb_data.mounta_iface = NULL;
    dbus_usb_data.signal_data = signal_data;

    const struct dbus_register_submodule_t self =
    {
        connect_to_session_bus,
        &dbus_usb_data,
        nullptr,
        connect_dbus_signals,
        nullptr,
        shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbusMounTA *dbus_usb_get_mounta_iface(void)
{
    return dbus_usb_data.mounta_iface;
}
