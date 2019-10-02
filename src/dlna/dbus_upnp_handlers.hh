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

#ifndef DBUS_UPNP_HANDLERS_HH
#define DBUS_UPNP_HANDLERS_HH

#include "upnp_listtree.hh"

#include <gio/gio.h>

/*!
 * \addtogroup dbus
 */
/*!@{*/

namespace DBusUPnP
{

/*!
 * \addtogroup dbus_handlers_dlna Handlers for com.intel.dLeynaServer.Manager interface.
 * \ingroup dbus_handlers
 */
/*!@{*/

/*!
 * Structure passed to D-Bus signal handlers concerning UPnP.
 */
struct SignalData
{
    UPnP::ListTree &upnp_list_tree_;

    explicit SignalData(UPnP::ListTree &upnp_list_tree):
        upnp_list_tree_(upnp_list_tree)
    {}
};

void dleynaserver_manager_signal(GDBusProxy *proxy, const gchar *sender_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 SignalData *data);
void dleynaserver_vanished(SignalData *data);

/*!@}*/

}

/*!@}*/

#endif /* !DBUS_UPNP_HANDLERS_HH */
