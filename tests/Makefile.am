#
# Copyright (C) 2015, 2017--2020, 2022  T+A elektroakustik GmbH & Co. KG
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

if WITH_CUTTER
TESTS = run_test.sh

if WITH_VALGRIND
TESTS += valgrind.sh
endif

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)" XSLTPROC="$(XSLTPROC)" VALGRIND="$(VALGRIND)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src/common -I$(top_builddir)/src/common
AM_CPPFLAGS += -I$(DBUS_IFACES)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = \
    test_lru.la \
    test_lru_upnp.la \
    test_listtree_upnp.la \
    test_cacheable_overrides.la \
    test_readyprobes.la \
    test_urlschemes.la \
    test_md5.la

test_lru_la_SOURCES = \
    test_lru.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_timebase.hh
test_lru_la_LIBADD = $(top_builddir)/src/common/liblru.la
test_lru_la_CFLAGS = $(AM_CFLAGS)
test_lru_la_CXXFLAGS = $(AM_CXXFLAGS)

test_lru_upnp_la_SOURCES = \
    test_lru_upnp.cc mock_expectation.hh \
    fake_dbus.hh \
    mock_dbus_upnp_helpers.hh mock_dbus_upnp_helpers.cc \
    mock_upnp_dleynaserver_dbus.hh mock_upnp_dleynaserver_dbus.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_timebase.hh
test_lru_upnp_la_LIBADD = $(top_builddir)/src/common/liblru.la $(top_builddir)/src/dlna/libupnp_list.la
test_lru_upnp_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/dlna -I$(top_builddir)/src/dlna
test_lru_upnp_la_CFLAGS = $(AM_CFLAGS) $(LISTBROKER_DEPENDENCIES_CFLAGS)
test_lru_upnp_la_CXXFLAGS = $(AM_CXXFLAGS) $(LISTBROKER_DEPENDENCIES_CFLAGS)

test_listtree_upnp_la_SOURCES = \
    test_listtree_upnp.cc mock_expectation.hh \
    fake_dbus.hh \
    mock_dbus_upnp_helpers.hh mock_dbus_upnp_helpers.cc \
    mock_dbus_lists_iface.hh mock_dbus_lists_iface.cc \
    mock_listbrokers_dbus.hh mock_listbrokers_dbus.cc \
    mock_upnp_dleynaserver_dbus.hh mock_upnp_dleynaserver_dbus.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_timebase.hh
test_listtree_upnp_la_LIBADD = \
    $(top_builddir)/src/common/liblru.la \
    $(top_builddir)/src/common/liblisttree.la \
    $(top_builddir)/src/common/libdbus_asyncwork.la \
    $(top_builddir)/src/dlna/libupnp_list.la
test_listtree_upnp_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/dlna -I$(top_builddir)/src/dlna
test_listtree_upnp_la_CFLAGS = $(AM_CFLAGS) $(LISTBROKER_DEPENDENCIES_CFLAGS)
test_listtree_upnp_la_CXXFLAGS = $(AM_CXXFLAGS) $(LISTBROKER_DEPENDENCIES_CFLAGS)

test_cacheable_overrides_la_SOURCES = \
    test_cacheable_overrides.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_timebase.hh
test_cacheable_overrides_la_LIBADD = \
    $(top_builddir)/src/common/liblru.la \
    $(top_builddir)/src/common/libcacheable.la
test_cacheable_overrides_la_CFLAGS = $(AM_CFLAGS)
test_cacheable_overrides_la_CXXFLAGS = $(AM_CXXFLAGS)

test_readyprobes_la_SOURCES = test_readyprobes.cc
test_readyprobes_la_CFLAGS = $(AM_CFLAGS)
test_readyprobes_la_CXXFLAGS = $(AM_CXXFLAGS)

test_urlschemes_la_SOURCES = \
    test_urlschemes.cc \
    mock_messages.hh mock_messages.cc
test_urlschemes_la_LIBADD = \
    $(top_builddir)/src/usb/libusb_strbourl.la \
    $(top_builddir)/src/common/libstrbourl.la
test_urlschemes_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/usb
test_urlschemes_la_CFLAGS = $(AM_CFLAGS)
test_urlschemes_la_CXXFLAGS = $(AM_CXXFLAGS)

test_md5_la_SOURCES = test_md5.cc
test_md5_la_LIBADD = $(top_builddir)/src/common/libmd5.la
test_md5_la_CFLAGS = $(AM_CFLAGS)
test_md5_la_CXXFLAGS = $(AM_CXXFLAGS)

CLEANFILES = test_report.xml test_report_junit.xml valgrind.xml

EXTRA_DIST = cutter2junit.xslt
EXTRA_DIST += cutter-1_2_4.supp

check-local: check-TESTS
	@if $(GREP) -w cutter $(TEST_LOGS); then \
	    echo "Unit tests failed (check log)"; \
	    exit 1; \
	fi
endif
