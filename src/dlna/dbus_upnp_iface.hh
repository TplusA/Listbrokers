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

#ifndef DBUS_UPNP_IFACE_H
#define DBUS_UPNP_IFACE_H

#include "dbus_upnp_handlers.hh"

/*!
 * \addtogroup dbus
 */
/*!@{*/

namespace DBusUPnP
{

void dbus_setup(bool connect_to_session_bus, const char *dbus_object_path,
                SignalData *signal_data,
                void (*dleyna_status_watcher)(bool, void *),
                void *dleyna_status_watcher_data);

}

/*!@}*/

#endif /* !DBUS_UPNP_IFACE_H */
