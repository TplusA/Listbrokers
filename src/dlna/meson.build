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

dleynaserver_dbus_src = gnome.gdbus_codegen('com_intel_dleynaserver',
    sources: '../../dbus_interfaces/com_intel_dleynaserver.xml',
    interface_prefix: 'com.intel.dLeynaServer.',
    namespace: 'tdbus_dleynaserver')
dleynaserver_dbus_lib = static_library('dleynaserver',
    dleynaserver_dbus_src,
    include_directories: '../common',
    dependencies: glib_deps, c_args: relaxed_dbus_warnings)
dleynaserver_dbus_dep = declare_dependency(link_with: dleynaserver_dbus_lib)
dbus_docs += custom_target('dleynaserver_docs',
    input: '../../dbus_interfaces/com_intel_dleynaserver.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_dleynaserver', '-s', 'com.intel.dLeynaServer.',
        '-n', 'DLNA Server Management'
    ])

upnp_media_dbus_src = gnome.gdbus_codegen('org_gnome_upnp',
    sources: '../../dbus_interfaces/org_gnome_upnp.xml',
    interface_prefix: 'org.gnome.UPnP.',
    namespace: 'tdbus_upnp')
upnp_media_dbus_lib = static_library('upnp_media',
    upnp_media_dbus_src,
    include_directories: '../common',
    dependencies: glib_deps, c_args: relaxed_dbus_warnings)
upnp_media_dbus_dep = declare_dependency(link_with: upnp_media_dbus_lib)
dbus_docs += custom_target('upnp_media_docs',
    input: '../../dbus_interfaces/org_gnome_upnp.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_upnp', '-s', 'org.gnome.UPnP.',
        '-n', 'DLNA Server Management'
    ])

upnp_list_lib = static_library('upnp_list',
    ['upnp_list.cc', 'upnp_listtree.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

dbus_upnp_handlers_lib = static_library('dbus_upnp_handlers',
    'dbus_upnp_handlers.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h])

dbus_upnp_helpers_lib = static_library('dbus_upnp_helpers',
    ['dbus_upnp_helpers.cc', 'dbus_upnp_list_filler.cc'],
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
        '../common/os.c',
        '../common/urlstring.cc',
        version_info
    ],
    include_directories: dbus_iface_defs_includes,
    dependencies: [
        glib_deps,
        artcache_dbus_dep,
        debug_dbus_dep,
        dleynaserver_dbus_dep,
        lists_dbus_dep,
        upnp_media_dbus_dep,
        config_h,
    ],
    link_with: [
        dbus_artcache_iface_lib,
        dbus_lists_handlers_lib,
        dbus_lists_iface_lib,
        dbus_upnp_handlers_lib,
        dbus_upnp_helpers_lib,
        listtree_lib,
        lru_lib,
        md5_lib,
        upnp_list_lib,
    ],
    install: true
)
