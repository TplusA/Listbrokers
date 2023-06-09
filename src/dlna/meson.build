#
# Copyright (C) 2019, 2020, 2021, 2022, 2023  T+A elektroakustik GmbH & Co. KG
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
    ['com_intel_dleynaserver', 'com.intel.dLeynaServer.', 'tdbus_dleynaserver', 'DLNA Server Management'],
    ['org_gnome_upnp',         'org.gnome.UPnP.',         'tdbus_upnp',         'DLNA Access'],
]

dbus_dlna_headers = []
dbus_dlna_deps = []
dbus_dlna_toc_inputs = []

foreach d : dbus_iface_data
    codegen = gnome.gdbus_codegen(d[0],
                                sources: dbus_iface_dir / d[0] + '.xml',
                                interface_prefix: d[1],
                                namespace: d[2])
    dbus_dlna_headers += codegen[1]

    dbus_dlna_deps += declare_dependency(
        link_with: static_library(
            d[0].split('_')[-1] + '_dbus', codegen,
            dependencies: [glib_deps, config_h],
            c_args: relaxed_dbus_warnings
        ),
        include_directories: dbus_iface_defs_includes
    )

    doc_target = custom_target(d[0] + '_docs',
        input: dbus_iface_dir / d[0] + '.xml',
        output: ['@BASENAME@-doc.mdp', '@BASENAME@-doc.h'],
        command: [
            extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
            '-c', d[2], '-s', d[1], '-n', d[3]
        ])

    dbus_dlna_toc_inputs += doc_target[0]
    dbus_docs += doc_target
    doc_target = []
    codegen = []
endforeach

dbus_docs += custom_target('all_interfaces_toc.md',
    input: dbus_dlna_toc_inputs,
    output: ['all_interfaces_toc.md', 'all_interfaces.md'],
    command: [
        combine_iface_pages, '-toc', '@OUTPUT0@', '-ifaces', '@OUTPUT1@', '@INPUT@'
    ]
)

upnp_list_lib = static_library('upnp_list',
    ['upnp_list.cc', 'upnp_listtree.cc', dbus_dlna_headers],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

dbus_upnp_handlers_lib = static_library('dbus_upnp_handlers',
    'dbus_upnp_handlers.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

dbus_upnp_helpers_lib = static_library('dbus_upnp_helpers',
    ['dbus_upnp_helpers.cc', 'dbus_upnp_list_filler.cc', dbus_dlna_headers],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

executable(
    'strbo_lb_upnp',
    [
        'strbo_lb_upnp.cc',
        'dbus_upnp_iface.cc',
        'periodic_rescan.cc',
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
        dbus_common_deps,
        dbus_dlna_deps,
        config_h,
    ],
    link_with: [
        dbus_artcache_iface_lib,
        dbus_lists_handlers_lib,
        dbus_lists_iface_lib,
        dbus_asyncwork_lib,
        dbus_error_messages_lib,
        dbus_upnp_handlers_lib,
        dbus_upnp_helpers_lib,
        debug_dbus_lib,
        listtree_lib,
        lru_lib,
        md5_lib,
        upnp_list_lib,
    ],
    install: true
)
