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

#ifndef DBUS_UPNP_HANDLERS_HH
#define DBUS_UPNP_HANDLERS_HH

#include "dbus_upnp_handlers.h"
#include "upnp_listtree.hh"

/*!
 * Structure passed to D-Bus signal handlers concerning UPnP.
 *
 * This structure is only usable in C++, but the C part needs to know its name
 * to pass it on in a safe way. In pure C code, only an opaque pointer type to
 * \c struct \c DBusUPnPSignalData is declared. In C++, the structure
 * itself may be used.
 */
struct DBusUPnPSignalData
{
    UPnP::ListTree &upnp_list_tree_;

    explicit DBusUPnPSignalData(UPnP::ListTree &upnp_list_tree):
        upnp_list_tree_(upnp_list_tree)
    {}
};

#endif /* !DBUS_UPNP_HANDLERS_HH */
