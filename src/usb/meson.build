#
# Copyright (C) 2019, 2020, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

dbus_iface_data = [
    ['de_tahifi_mounta', 'de.tahifi.', 'tdbus', 'T+A Automounter'],
]

dbus_usb_deps = []

foreach d : dbus_iface_data
    dbus_usb_deps += declare_dependency(
        link_with: static_library(
            d[0].split('_')[-1] + '_dbus',
            gnome.gdbus_codegen(d[0],
                                sources: dbus_iface_dir / d[0] + '.xml',
                                interface_prefix: d[1],
                                namespace: d[2]),
            dependencies: [glib_deps, config_h],
            c_args: relaxed_dbus_warnings),
        include_directories: dbus_iface_defs_includes
    )

    dbus_docs += custom_target(d[0] + '_docs',
        input: dbus_iface_dir / d[0] + '.xml',
        output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
        command: [
            extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
            '-c', d[2], '-s', d[1], '-n', d[3]
        ])
endforeach

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
        'dbus_usb_iface.cc',
        'usb_helpers.cc',
        '../common/main.cc',
        '../common/cachecontrol.cc',
        '../common/dbus_common.c',
        '../common/messages.c',
        '../common/messages_glib.c',
        '../common/backtrace.c',
        '../common/os.c',
        '../common/urlstring.cc',
        version_info
    ],
    include_directories: dbus_iface_defs_includes,
    dependencies: [
        glib_deps,
        dependency('curlpp'),
        dbus_common_deps,
        dbus_usb_deps,
        config_h,
    ],
    link_with: [
        cacheable_lib,
        dbus_artcache_iface_lib,
        dbus_lists_handlers_lib,
        dbus_lists_iface_lib,
        dbus_asyncwork_lib,
        dbus_error_messages_lib,
        dbus_mounta_handlers_lib,
        debug_dbus_lib,
        listtree_lib,
        lru_lib,
        md5_lib,
        strbourl_lib,
        usb_list_lib,
        usb_strbourl_lib,
    ],
    install: true
)
