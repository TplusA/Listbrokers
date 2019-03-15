/*
 * Copyright (C) 2015, 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <iostream>

#include "main.hh"

#include "dbus_upnp_iface.h"
#include "dbus_upnp_handlers.hh"
#include "periodic_rescan.hh"
#include "messages_glib.h"
#include "versioninfo.h"

class UPnPListTreeData: public ListTreeData
{
  public:
    std::unique_ptr<UPnP::ListTree> list_tree_;
    std::unique_ptr<Cacheable::CheckNoOverrides> cache_check_;

    ~UPnPListTreeData() {}

    ListTreeIface &get_list_tree() const override
    {
        return *list_tree_;
    }
};

class UPnPDBusData: public DBusData
{
  public:
    std::unique_ptr<struct DBusUPnPSignalData> upnp_signal_data_;

    ~UPnPDBusData() {}

    int init(ListTreeData &ltd) override
    {
        int ret = DBusData::init(ltd);

        if(ret < 0)
            return ret;

        upnp_signal_data_.reset(new DBusUPnPSignalData(*static_cast<UPnPListTreeData &>(ltd).list_tree_.get()));

        if(upnp_signal_data_ == nullptr)
            return msg_out_of_memory("D-Bus UPnP signal data");

        return 0;
    }
};

const char DBusData::dbus_bus_name_[] = "de.tahifi.UPnPBroker";
const char DBusData::dbus_object_path_[] = "/de/tahifi/UPnPBroker";

static void show_version_info(void)
{
    printf("%s -- UPnP AV\n"
           "Revision %s%s\n"
           "         %s+%d, %s\n",
           PACKAGE_STRING,
           VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
           VCS_TAG, VCS_TICK, VCS_DATE);
}

void LBApp::log_version_info(void)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Rev %s%s, %s+%d, %s",
              VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
              VCS_TAG, VCS_TICK, VCS_DATE);
}

static int create_list_tree_and_cache(UPnPListTreeData &lt, GMainLoop *loop)
{
    static constexpr size_t default_maximum_size_mib = 20UL * 1024UL * 1024UL;
    static constexpr size_t default_maximum_number_of_lists = 10000;

    lt.cache_.reset(new LRU::Cache(default_maximum_size_mib,
                                   default_maximum_number_of_lists,
                                   std::chrono::minutes(15)));
    if(lt.cache_ == nullptr)
        return msg_out_of_memory("LRU cache");

    lt.cache_control_.reset(new LRU::CacheControl(*lt.cache_, loop));
    if(lt.cache_control_ == nullptr)
        return msg_out_of_memory("LRU cache control");

    lt.cache_check_.reset(new Cacheable::CheckNoOverrides());
    if(lt.cache_check_ == nullptr)
        return msg_out_of_memory("Cacheable check");

    UPnP::init_standard_dbus_fillers(*lt.cache_);

    lt.list_tree_.reset(new UPnP::ListTree(*lt.cache_, *lt.cache_check_));
    if(lt.list_tree_ == nullptr)
        return msg_out_of_memory("UPnP list tree");

    lt.cache_->set_callbacks([&lt] { lt.cache_control_->enable_garbage_collection(); },
                             [&lt] { lt.cache_control_->trigger_gc(); },
                             [&lt] (ID::List id) { lt.list_tree_->list_discarded_from_cache(id); },
                             [&lt] { lt.cache_control_->disable_garbage_collection(); });
    lt.list_tree_->init();

    return 0;
}

static void usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
           "\n"
           "Options:\n"
           "  --help         Show this help.\n"
           "  --version      Print version information to stdout.\n"
           "  --stderr       Write log messages to stderr, not syslog.\n"
           "  --verbose lvl  Set verbosity level to given level.\n"
           "  --quiet        Short for \"--verbose quite\".\n"
           ;
}

static int process_command_line(int argc, char *argv[],
                                enum MessageVerboseLevel &verbose_level,
                                bool &syslog_to_stderr)
{
    verbose_level = MESSAGE_LEVEL_NORMAL;
    syslog_to_stderr = false;

#define CHECK_ARGUMENT() \
    do \
    { \
        if(i + 1 >= argc) \
        { \
            std::cerr << "Option " << argv[i] << " requires an argument." << std::endl; \
            return -1; \
        } \
        ++i; \
    } \
    while(0)

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--stderr") == 0)
            syslog_to_stderr = true;
        else if(strcmp(argv[i], "--verbose") == 0)
        {
            CHECK_ARGUMENT();
            verbose_level = msg_verbose_level_name_to_level(argv[i]);

            if(verbose_level == MESSAGE_LEVEL_IMPOSSIBLE)
            {
                fprintf(stderr,
                        "Invalid verbosity \"%s\". "
                        "Valid verbosity levels are:\n", argv[i]);

                const char *const *names = msg_get_verbose_level_names();

                for(const char *name = *names; name != NULL; name = *++names)
                    fprintf(stderr, "    %s\n", name);

                return -1;
            }
        }
        else if(strcmp(argv[i], "--quiet") == 0)
            verbose_level = MESSAGE_LEVEL_QUIET;
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help." << std::endl;
            return -1;
        }
    }

#undef CHECK_ARGUMENT

    return 0;
}

int LBApp::startup(int argc, char *argv[])
{
    enum MessageVerboseLevel verbose_level;
    bool syslog_to_stderr;
    int ret = process_command_line(argc, argv, verbose_level, syslog_to_stderr);

    if(ret == -1)
        return -1;
    else if(ret == 1)
    {
        usage(argv[0]);
        return 1;
    }
    else if(ret == 2)
    {
        show_version_info();
        return 1;
    }

    msg_enable_syslog(!syslog_to_stderr);
    msg_enable_glib_message_redirection();
    msg_set_verbose_level(verbose_level);

    return 0;
}

static UPnPListTreeData global_ltd;
static UPnPDBusData global_dbd;

ListTreeData &LBApp::get_list_tree_data_singleton()
{
    return global_ltd;
}

int LBApp::setup_application_data(DBusData *&dbus_data, ListTreeData *&lt_data,
                                  GMainLoop *loop)
{
    if(create_list_tree_and_cache(global_ltd, loop) < 0)
        return -1;

    dbus_data = &global_dbd;
    lt_data = &global_ltd;

    return 0;
}

static void dleyna_status_watcher(bool is_available, void *user_data)
{
    auto *const periodic_rescan = static_cast<UPnP::PeriodicRescan *>(user_data);

    if(is_available)
        periodic_rescan->enable();
    else
        periodic_rescan->disable();
}

void LBApp::dbus_setup(DBusData &dbd)
{
    static std::unique_ptr<UPnP::PeriodicRescan> periodic_rescan;
    periodic_rescan = std::make_unique<UPnP::PeriodicRescan>(10 * 60);

    dbus_upnp_setup(true, dbd.dbus_object_path_,
                    static_cast<UPnPDBusData &>(dbd).upnp_signal_data_.get(),
                    dleyna_status_watcher, periodic_rescan.get());
}
