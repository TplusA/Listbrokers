/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_mounta_handlers.hh"
#include "dbus_common.h"
#include "messages.h"

void dbussignal_mounta(GDBusProxy *proxy, const gchar *sender_name,
                       const gchar *signal_name, GVariant *parameters,
                       gpointer user_data)
{
    log_assert(user_data != NULL);

    auto *const data = static_cast<struct DBusMounTASignalData *>(user_data);

    static const char iface_name[] = "de.tahifi.MounTA";

    msg_vinfo(MESSAGE_LEVEL_TRACE,
              "%s signal from '%s': %s", iface_name, sender_name, signal_name);

    if(strcmp(signal_name, "DeviceRemoved") == 0)
    {
        uint16_t device_id;
        const char *rootpath;

        g_variant_get(parameters, "(q&s)", &device_id, &rootpath);

        auto dev_list = data->usb_list_tree_.get_list_of_usb_devices();

        ID::List removed_list_id;
        if(dev_list->remove_from_list(device_id, removed_list_id))
            data->usb_list_tree_.purge_device_subtree_and_reinsert_device_list(removed_list_id);
    }
    else if(strcmp(signal_name, "NewUSBDevice") == 0)
    {
        uint16_t device_id;
        const char *devname;
        const char *rootpath;
        const char *usb_port;

        g_variant_get(parameters, "(q&s&s&s)",
                      &device_id, &devname, &rootpath, &usb_port);

        auto dev_list = data->usb_list_tree_.get_list_of_usb_devices();

        if(dev_list->add_to_list(device_id, devname, usb_port))
            data->usb_list_tree_.reinsert_device_list();
        else
            msg_info("Not inserting USB device %u (%s) again",
                     device_id, devname);
    }
    else if(strcmp(signal_name, "NewVolume") == 0)
    {
        uint32_t number;
        const char *label;
        const char *mountpoint;
        uint16_t device_id;

        g_variant_get(parameters, "(u&s&sq)", &number, &label, &mountpoint, &device_id);

        auto dev_list = data->usb_list_tree_.get_list_of_usb_devices();
        auto *referenced_device = dev_list->get_device_by_id(device_id);

        if(referenced_device == nullptr)
            msg_error(0, LOG_ERR, "Received volume %u \"%s\" on non-existent "
                      "device ID %u from MounTA", number, label, device_id);
        else
        {
            size_t added_at_index;

            if(referenced_device->add_volume(number, label, mountpoint,
                                             added_at_index))
                data->usb_list_tree_.reinsert_volume_list(device_id, number,
                                                          added_at_index);
        }
    }
    else
        dbus_common_unknown_signal(iface_name, signal_name, sender_name);
}
