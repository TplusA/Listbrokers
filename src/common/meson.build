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

config_h = declare_dependency(
    sources: configure_file(input: '../../config.h.meson',
                            output: 'config.h',
                            configuration: config_data),
    include_directories: '.')

gnome = import('gnome')

dbus_docs = []

dbus_iface_defs_includes = include_directories('../../dbus_interfaces')

artcache_dbus_src = gnome.gdbus_codegen('de_tahifi_artcache',
    sources: '../../dbus_interfaces/de_tahifi_artcache.xml',
    interface_prefix: 'de.tahifi.ArtCache.',
    namespace: 'tdbus_artcache')
artcache_dbus_lib = static_library('artcache_dbus',
    artcache_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
artcache_dbus_dep = declare_dependency(link_with: artcache_dbus_lib)
dbus_docs += custom_target('de_tahifi_artcache_docs',
    input: '../../dbus_interfaces/de_tahifi_artcache.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_artcache', '-s', 'de.tahifi.ArtCache.', '-n', 'Cover Art'
    ])

debug_dbus_src = gnome.gdbus_codegen('de_tahifi_debug',
    sources: '../../dbus_interfaces/de_tahifi_debug.xml',
    interface_prefix: 'de.tahifi.Debug.',
    namespace: 'tdbus_debug')
debug_dbus_lib = static_library('debug_dbus',
    [debug_dbus_src, 'dbus_debug_levels.c', 'messages_dbus.c'],
    dependencies: glib_deps, c_args: relaxed_dbus_warnings)
debug_dbus_dep = declare_dependency(link_with: debug_dbus_lib)
dbus_docs += custom_target('de_tahifi_debug_docs',
    input: '../../dbus_interfaces/de_tahifi_debug.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_debug', '-s', 'de.tahifi.Debug.', '-n', 'Introspection'
    ])

lists_dbus_src = gnome.gdbus_codegen('de_tahifi_lists',
    sources: '../../dbus_interfaces/de_tahifi_lists.xml',
    interface_prefix: 'de.tahifi.Lists.',
    namespace: 'tdbus_lists')
lists_dbus_lib = static_library('lists_dbus',
    lists_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
lists_dbus_dep = declare_dependency(link_with: lists_dbus_lib)
dbus_docs += custom_target('de_tahifi_lists_docs',
    input: '../../dbus_interfaces/de_tahifi_lists.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_lists', '-s', 'de.tahifi.Lists.', '-n', 'Content Browsing'
    ])

mounta_dbus_src = gnome.gdbus_codegen('de_tahifi_mounta',
    sources: '../../dbus_interfaces/de_tahifi_mounta.xml',
    interface_prefix: 'de.tahifi.',
    namespace: 'tdbus')
mounta_dbus_lib = static_library('mounta_dbus',
    mounta_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
mounta_dbus_dep = declare_dependency(link_with: mounta_dbus_lib)
dbus_docs += custom_target('de_tahifi_mounta_docs',
    input: '../../dbus_interfaces/de_tahifi_mounta.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus', '-s', 'de.tahifi.', '-n', 'T+A Automounter'
    ])

lru_lib = static_library('lru',
    'lru.cc',
    include_directories: dbus_iface_defs_includes)

strbourl_lib = static_library('strbourl',
    'strbo_url_common.cc',
    include_directories: dbus_iface_defs_includes)

listtree_lib = static_library('listtree',
    ['listtree_manager.cc', 'dbus_async_work.cc'],
    dependencies: [glib_deps, lists_dbus_dep],
    include_directories: dbus_iface_defs_includes)

cacheable_lib = static_library('cacheable',
    'cacheable.cc',
    include_directories: dbus_iface_defs_includes)

dbus_lists_handlers_lib = static_library('dbus_lists_handlers',
    ['dbus_lists_handlers.cc', 'listtree_glue.cc'],
    dependencies: [glib_deps, artcache_dbus_dep],
    include_directories: dbus_iface_defs_includes)

dbus_lists_iface_lib = static_library('dbus_lists_iface',
    'dbus_lists_iface.c',
    dependencies: glib_deps)

dbus_artcache_iface_lib = static_library('dbus_artcache_iface',
    'dbus_artcache_iface.c',
    dependencies: glib_deps)

md5_lib = static_library('md5', 'md5.cc')

inifile_lib = static_library('inifile', 'inifile.c')
