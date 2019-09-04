#
# Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
#
# This file is part of T+A List Brokers.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

dbus_iface_defs_includes = include_directories('../../dbus_interfaces')

usb_list_lib = static_library('usb_list',
    ['usb_list.cc', 'usb_listtree.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

usb_strbourl_lib = static_library('usb_strbourl',
    'strbo_url_usb.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: config_h)

dbus_mounta_handlers_lib = static_library('dbus_mounta_handlers',
    'dbus_mounta_handlers.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

executable(
    'strbo_lb_usb',
    [
        'strbo_lb_usb.cc',
        'dbus_usb_iface.c',
        'usb_helpers.cc',
        '../common/main.cc',
        '../common/cachecontrol.cc',
        '../common/dbus_common.c',
        '../common/messages.c',
        '../common/messages_glib.c',
        '../common/os.c',
        '../common/urlstring.cc',
        version_info
    ],
    include_directories: dbus_iface_defs_includes,
    dependencies: [
        glib_deps,
        dependency('curlpp'),
        artcache_dbus_dep,
        debug_dbus_dep,
        lists_dbus_dep,
        mounta_dbus_dep,
        config_h,
    ],
    link_with: [
        cacheable_lib,
        dbus_artcache_iface_lib,
        dbus_lists_handlers_lib,
        dbus_lists_iface_lib,
        dbus_mounta_handlers_lib,
        listtree_lib,
        lru_lib,
        md5_lib,
        strbourl_lib,
        usb_list_lib,
        usb_strbourl_lib,
    ],
    install: true
)
