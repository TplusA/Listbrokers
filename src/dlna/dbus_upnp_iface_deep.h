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

#ifndef DBUS_UPNP_IFACE_DEEP_H
#define DBUS_UPNP_IFACE_DEEP_H

#include "com_intel_dleynaserver.h"
#include "org_gnome_upnp.h"

#ifdef __cplusplus
extern "C" {
#endif

tdbusdleynaserverManager *dbus_upnp_get_dleynaserver_manager_iface(void);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_UPNP_IFACE_DEEP_H */
