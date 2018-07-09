/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_USB_IFACE_H
#define DBUS_USB_IFACE_H

#include <stdbool.h>

#include "dbus_mounta_handlers.h"

#ifdef __cplusplus
extern "C" {
#endif

void dbus_mounta_setup(bool connect_to_session_bus, const char *dbus_object_path,
                       struct DBusMounTASignalData *signal_data);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_USB_IFACE_H */
