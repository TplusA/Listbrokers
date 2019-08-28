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

#include "dbus_lists_handlers.h"
#include "listtree.hh"

/*!
 * Structure passed to D-Bus method handlers concerning list navigation.
 *
 * This structure is only usable in C++, but the C part needs to know its name
 * to pass it on in a safe way. In pure C code, only an opaque pointer type to
 * \c struct \c DBusNavlistsIfaceData is declared. In C++, the structure
 * itself may be used.
 */
struct DBusNavlistsIfaceData
{
    ListTreeIface &listtree_;

    explicit DBusNavlistsIfaceData(ListTreeIface &lt):
        listtree_(lt)
    {}
};

#endif /* !DBUS_LISTS_HANDLERS_HH */
