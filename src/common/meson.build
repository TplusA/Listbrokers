#
# Copyright (C) 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

config_h = declare_dependency(
    sources: configure_file(input: '../../config.h.meson',
                            output: 'config.h',
                            configuration: config_data),
    include_directories: '.')

gnome = import('gnome')

dbus_docs = []

dbus_iface_dir = '../../dbus_interfaces'
dbus_iface_defs_includes = include_directories(dbus_iface_dir)

dbus_iface_data = [
    ['de_tahifi_lists',    'de.tahifi.Lists.',    'tdbus_lists',    'Content Browsing'],
    ['de_tahifi_artcache', 'de.tahifi.ArtCache.', 'tdbus_artcache', 'Cover Art'],
    ['de_tahifi_debug',    'de.tahifi.Debug.',    'tdbus_debug',    'Debug Levels'],
    ['de_tahifi_errors',   'de.tahifi.',          'tdbus',          'Error Messages'],
]

dbus_common_deps = []

foreach d : dbus_iface_data
    dbus_common_deps += declare_dependency(
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

lru_lib = static_library('lru',
    'lru.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: config_h,
)

strbourl_lib = static_library('strbourl',
    'strbo_url_common.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: config_h,
)

listtree_lib = static_library('listtree',
    ['listtree_manager.cc', 'dbus_async_workqueue.cc', 'dbus_async_work.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, dbus_common_deps],
)

cacheable_lib = static_library('cacheable',
    'cacheable.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: config_h,
)

dbus_lists_handlers_lib = static_library('dbus_lists_handlers',
    ['dbus_lists_handlers.cc', 'listtree_glue.cc', 'gvariantwrapper.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, dbus_common_deps],
)

debug_dbus_lib = static_library('debug_dbus_more',
    ['dbus_debug_levels.cc', 'messages_dbus.c'],
    dependencies: [glib_deps, dbus_common_deps],
)

dbus_error_messages_lib = static_library('dbus_errors_iface',
    ['dbus_error_messages.cc'],
    dependencies: [glib_deps, dbus_common_deps],
)

dbus_lists_iface_lib = static_library('dbus_lists_iface',
    'dbus_lists_iface.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h],
)

dbus_artcache_iface_lib = static_library('dbus_artcache_iface',
    'dbus_artcache_iface.cc',
    dependencies: [glib_deps, dbus_common_deps],
)

md5_lib = static_library('md5', 'md5.cc')

inifile_lib = static_library('inifile', 'inifile.c')
