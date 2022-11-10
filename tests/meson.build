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

cutter_dep = dependency('cppcutter', required: false)
compiler = meson.get_compiler('cpp')

if not(compiler.has_header('cutter.h', dependencies: cutter_dep) and \
       compiler.has_header('cppcutter.h', dependencies: cutter_dep))
    subdir_done()
endif

cutter_wrap = find_program('cutter_wrap.sh')

cutter_wrap_args = [
    find_program('cutter').path(), find_program('xsltproc').path(),
    meson.current_build_dir(), meson.current_source_dir()
]

cacheable_overrides_tests = shared_module('test_cacheable_overrides',
    ['test_cacheable_overrides.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
    cpp_args: ['-Wno-pedantic', '-Wno-clobbered'],
    include_directories: ['../src/common', '../dbus_interfaces'],
    dependencies: cutter_dep,
    link_with: [lru_lib, cacheable_lib]
)
test('Cacheable Overrides',
    cutter_wrap, args: [cutter_wrap_args, cacheable_overrides_tests.full_path()],
    depends: cacheable_overrides_tests
)

listtree_upnp_tests = shared_module('test_listtree_upnp',
    ['test_listtree_upnp.cc', 'mock_dbus_upnp_helpers.cc',
     'mock_dbus_lists_iface.cc', 'mock_listbrokers_dbus.cc',
     'mock_upnp_dleynaserver_dbus.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
    cpp_args: ['-Wno-pedantic', '-Wno-clobbered'],
    include_directories: ['../src/common', '../src/dlna', '../dbus_interfaces'],
    dependencies: [cutter_dep, glib_deps],
    link_with: [lru_lib, upnp_list_lib, listtree_lib, dbus_asyncwork_lib]
)
test('UPnP List Tree',
    cutter_wrap, args: [cutter_wrap_args, listtree_upnp_tests.full_path()],
    depends: listtree_upnp_tests
)

lru_tests = shared_module('test_lru',
    ['test_lru.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
    cpp_args: ['-Wno-pedantic', '-Wno-clobbered'],
    include_directories: ['../src/common', '../dbus_interfaces'],
    dependencies: cutter_dep,
    link_with: lru_lib
)
test('LRU Cache Implementation',
    cutter_wrap, args: [cutter_wrap_args, lru_tests.full_path()],
    depends: lru_tests
)

lru_upnp_tests = shared_module('test_lru_upnp',
    ['test_lru_upnp.cc', 'mock_dbus_upnp_helpers.cc',
     'mock_upnp_dleynaserver_dbus.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
    cpp_args: ['-Wno-pedantic', '-Wno-clobbered'],
    include_directories: ['../src/common', '../src/dlna', '../dbus_interfaces'],
    dependencies: [cutter_dep, glib_deps],
    link_with: [lru_lib, upnp_list_lib]
)
test('UPnP Connection to LRU Cache',
    cutter_wrap, args: [cutter_wrap_args, lru_upnp_tests.full_path()],
    depends: lru_upnp_tests
)

md5_tests = shared_module('test_md5',
    'test_md5.cc',
    cpp_args: '-Wno-pedantic',
    include_directories: '../src/common',
    dependencies: cutter_dep,
    link_with: md5_lib
)
test('MD5 Checksum Computation',
    cutter_wrap, args: [cutter_wrap_args, md5_tests.full_path()],
    depends: md5_tests
)

readyprobes_tests = shared_module('test_readyprobes',
    'test_readyprobes.cc',
    cpp_args: '-Wno-pedantic',
    include_directories: '../src/common',
    dependencies: cutter_dep
)
test('Ready Probes',
    cutter_wrap, args: [cutter_wrap_args, readyprobes_tests.full_path()],
    depends: readyprobes_tests
)

urlschemes_tests = shared_module('test_urlschemes',
    ['test_urlschemes.cc', 'mock_messages.cc'],
    cpp_args: '-Wno-pedantic',
    include_directories: ['../src/common', '../src/usb', '../dbus_interfaces'],
    dependencies: cutter_dep,
    link_with: [strbourl_lib, usb_strbourl_lib]
)
test('URL Schemes',
    cutter_wrap, args: [cutter_wrap_args, urlschemes_tests.full_path()],
    depends: urlschemes_tests
)
