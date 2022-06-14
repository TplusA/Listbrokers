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

#include <glib-unix.h>

#include "main.hh"
#include "dbus_artcache_iface.hh"
#include "dbus_debug_levels.hh"
#include "dbus_error_messages.hh"
#include "dbus_common.h"

static Timebase real_timebase;
Timebase *LRU::timebase = &real_timebase;

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;

#if LOGGED_LOCKS_ENABLED && LOGGED_LOCKS_THREAD_CONTEXTS
thread_local LoggedLock::Context LoggedLock::context;
#endif

static GMainLoop *create_glib_main_loop()
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    if(loop == NULL)
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");

    return loop;
}

int DBusData::init(ListTreeData &ltd)
{
    navlists_iface_data_ = std::make_unique<DBusNavlists::IfaceData>(ltd.get_list_tree());

    if(navlists_iface_data_ == nullptr)
        return msg_out_of_memory("D-Bus navigation interface data");
    else
        return 0;
}

static int initialize_dbus(DBusData &dbd, ListTreeData &ltd, GMainLoop *loop)
{
    if(dbd.init(ltd) < 0)
        return -1;

    DBusDebugLevels::dbus_setup(true, dbd.dbus_object_path_);
    DBusErrorMessages::dbus_setup(true, dbd.dbus_object_path_);
    DBusArtCache::dbus_setup(true);
    DBusNavlists::dbus_setup(true, dbd.dbus_object_path_, dbd.get_navlists_iface_data());
    LBApp::dbus_setup(dbd);
    dbus_common_setup(loop, dbd.dbus_bus_name_);

    return 0;
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

static void connect_unix_signals(GMainLoop *loop)
{
    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);
}

int main(int argc, char *argv[])
{
    int ret = LBApp::startup(argc, argv);

    if(ret < 0)
        return EXIT_FAILURE;
    else if(ret > 0)
        return EXIT_SUCCESS;

    std::set_terminate([]
    {
        const auto ce = std::current_exception();

        if(ce == nullptr)
            msg_error(0, LOG_EMERG, "Terminate without active exception");
        else
        {
            try
            {
                std::rethrow_exception(ce);
            }
            catch(const std::exception &e)
            {
                msg_error(0, LOG_EMERG, "Unhandled exception: %s", e.what());
            }
            catch(...)
            {
                msg_error(0, LOG_EMERG, "Unhandled exception: *** UNKNOWN ***");
            }
        }

        os_abort();
    });

    LBApp::log_version_info();

    static GMainLoop *loop = create_glib_main_loop();
    if(loop == NULL)
        return EXIT_FAILURE;

    DBusData *dbd;
    ListTreeData *ltd;

    if(LBApp::setup_application_data(dbd, ltd, loop) < 0)
        return EXIT_FAILURE;

    if(initialize_dbus(*dbd, *ltd, loop) < 0)
       return EXIT_FAILURE;

    connect_unix_signals(loop);

    ltd->get_list_tree().start_threads(4);
    ltd->get_list_tree().pre_main_loop();

    g_main_loop_run(loop);

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Shutting down");
    dbus_common_shutdown(loop);

    ltd->get_list_tree().shutdown_threads();
    ltd->shutdown();

    return EXIT_SUCCESS;
}
