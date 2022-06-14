/*
 * Copyright (C) 2015, 2017, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef MAIN_HH
#define MAIN_HH

#include "cachecontrol.hh"
#include "dbus_lists_handlers.hh"

/*!
 * Base class with data relevant for the tree of cached lists.
 */
class ListTreeData
{
  public:
    std::unique_ptr<LRU::Cache> cache_;
    std::unique_ptr<LRU::CacheControl> cache_control_;

  protected:
    explicit ListTreeData() {}

  public:
    ListTreeData(const ListTreeData &) = delete;
    ListTreeData &operator=(const ListTreeData &) = delete;

    virtual ~ListTreeData() {}

    virtual ListTreeIface &get_list_tree() const = 0;
    virtual void shutdown() = 0;
};

/*!
 * Base class with structures relevant for D-Bus communication.
 */
class DBusData
{
  protected:
    std::unique_ptr<DBusNavlists::IfaceData> navlists_iface_data_;

  public:
    static const char dbus_bus_name_[];
    static const char dbus_object_path_[];

  protected:
    explicit DBusData() {}

  public:
    DBusData(const DBusData &) = delete;
    DBusData &operator=(const DBusData &) = delete;

    virtual ~DBusData() {}

    virtual int init(ListTreeData &ltd);

    DBusNavlists::IfaceData *get_navlists_iface_data() const
    {
        return navlists_iface_data_.get();
    }
};

/*!
 * Glue functions for calling application-specific code.
 */
namespace LBApp
{

/*!
 * Print software version information to log.
 */
void log_version_info(void);

/*!
 * Application-specific command line processing and some early initialization.
 *
 * \retval 0  No error.
 * \retval 1  Terminate application with successful exit code.
 * \retval -1 Terminate application with error.
 *
 * \note
 *     This function may not block and should return as soon as possible.
 */
int startup(int argc, char *argv[]);

/*!
 * More initialization of the heavier kind.
 *
 * \param[out] dbus_data
 *     On success, a pointer to the application-specific #DBusData object is
 *     returned here.
 *
 * \param[out] lt_data
 *     On success, a pointer to the application-specific #ListTreeData object
 *     is returned here.
 *
 * \param loop
 *     The application's GLib main loop for D-Bus handling and other
 *     proccessing.
 *
 * \returns
 *     0 on success, -1 on error.
 *
 * \note
 *     This function may not block and should return as soon as possible.
 */
int setup_application_data(DBusData *&dbus_data, ListTreeData *&lt_data,
                           GMainLoop *loop);

/*!
 * Set up application-specific D-Bus connections.
 *
 * \param dbd
 *     Reference to the #DBusData object returned by #setup_application_data().
 *     This parameter is useful for injecting data for testing and also allows
 *     the application to narrow the scope of its (probably global) #DBusData
 *     object.
 */
void dbus_setup(DBusData &dbd);

ListTreeData &get_list_tree_data_singleton();

}

#endif /* !MAIN_HH */
