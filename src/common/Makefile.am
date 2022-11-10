#
# Copyright (C) 2015--2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

ACLOCAL_AMFLAGS = -I ../../m4

AM_CPPFLAGS = -I$(DBUS_IFACES)
AM_CPPFLAGS += $(LISTBROKER_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

EXTRA_DIST = cacheable_glib.hh ready.hh readyprobe.hh

noinst_LTLIBRARIES = \
    liblru.la \
    libstrbourl.la \
    liblisttree.la \
    libcacheable.la \
    libdbus_asyncwork.la \
    libdbus_lists_handlers.la \
    libdbus_lists_iface.la \
    libdbus_artcache_iface.la \
    libmd5.la \
    liblists_dbus.la \
    libartcache_dbus.la \
    liberrors_dbus.la \
    libdebug_dbus.la \
    libinifile.la

liblru_la_SOURCES = \
    lru.cc lru.hh lru_killed_lists.hh  timebase.hh messages.h idtypes.hh \
    $(DBUS_IFACES)/de_tahifi_lists_context.h
liblru_la_CFLAGS = $(AM_CFLAGS)
liblru_la_CXXFLAGS = $(AM_CXXFLAGS)

libstrbourl_la_SOURCES = \
    strbo_url.hh strbo_url_schemes.hh strbo_url_helpers.hh \
    strbo_url_common.cc \
    messages.h
libstrbourl_la_CFLAGS = $(AM_CFLAGS)
libstrbourl_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_asyncwork_la_SOURCES = \
    dbus_async_workqueue.hh dbus_async_workqueue.cc \
    dbus_async_work.hh dbus_async_work.cc \
    work_by_cookie.hh work_by_cookie.cc
libdbus_asyncwork_la_CFLAGS = $(AM_CFLAGS)
libdbus_asyncwork_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_lists_handlers_la_SOURCES = \
    dbus_lists_handlers.cc dbus_lists_handlers.hh \
    listtree_glue.hh listtree_glue.cc \
    gvariantwrapper.hh gvariantwrapper.cc \
    gerrorwrapper.hh \
    dbus_async_workqueue.hh dbus_async_work.hh \
    logged_lock.hh \
    urlstring.hh i18nstring.hh ranked_stream_links.hh md5.hh \
    listtree.hh
libdbus_lists_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_lists_handlers_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libdbus_lists_iface_la_SOURCES = \
    dbus_lists_iface.cc dbus_lists_iface.hh
libdbus_lists_iface_la_CFLAGS = $(CRELAXEDWARNINGS)
libdbus_lists_iface_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_artcache_iface_la_SOURCES = \
    dbus_artcache_iface.cc dbus_artcache_iface.hh dbus_artcache_iface_deep.h
libdbus_artcache_iface_la_CFLAGS = $(CRELAXEDWARNINGS)
libdbus_artcache_iface_la_CXXFLAGS = $(AM_CXXFLAGS)

liblisttree_la_SOURCES = \
    listtree_manager.cc listtree_manager.hh \
    cacheable.hh \
    dbus_async_workqueue.hh dbus_async_work.hh \
    logged_lock.hh \
    listtree.hh idtypes.hh \
    $(DBUS_IFACES)/de_tahifi_lists_context.h
liblisttree_la_CFLAGS = $(AM_CFLAGS)
liblisttree_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libcacheable_la_SOURCES = cacheable.cc cacheable.hh lru.hh
libcacheable_la_CFLAGS = $(AM_CFLAGS)
libcacheable_la_CXXFLAGS = $(AM_CXXFLAGS)

libmd5_la_SOURCES = md5.cc md5.hh
libmd5_la_CFLAGS = $(AM_CFLAGS)
libmd5_la_CXXFLAGS = $(AM_CXXFLAGS)

nodist_liblists_dbus_la_SOURCES = de_tahifi_lists.c de_tahifi_lists.h
liblists_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

nodist_libartcache_dbus_la_SOURCES = de_tahifi_artcache.c de_tahifi_artcache.h
libartcache_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

liberrors_dbus_la_SOURCES = dbus_error_messages.cc dbus_error_messages.hh
nodist_liberrors_dbus_la_SOURCES = de_tahifi_errors.c de_tahifi_errors.h
liberrors_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libdebug_dbus_la_SOURCES = \
    dbus_debug_levels.cc dbus_debug_levels.hh \
    gerrorwrapper.hh \
    messages_dbus.c messages_dbus.h
nodist_libdebug_dbus_la_SOURCES = de_tahifi_debug.c de_tahifi_debug.h
libdebug_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libinifile_la_SOURCES = inifile.c inifile.h
libinifile_la_CFLAGS = $(AM_CFLAGS)
libinifile_la_CXXFLAGS = $(AM_CXXFLAGS)

BUILT_SOURCES = \
    $(nodist_liblists_dbus_la_SOURCES) \
    de_tahifi_lists-doc.h \
    de_tahifi_lists-doc.md \
    de_tahifi_lists.stamp \
    $(nodist_libartcache_dbus_la_SOURCES) \
    de_tahifi_artcache-doc.h \
    de_tahifi_artcache-doc.md \
    de_tahifi_artcache.stamp \
    $(nodist_liberrors_dbus_la_SOURCES) \
    de_tahifi_errors-doc.h \
    de_tahifi_errors-doc.md \
    de_tahifi_errors.stamp \
    $(nodist_libdebug_dbus_la_SOURCES) \
    de_tahifi_debug-doc.h \
    de_tahifi_debug-doc.md \
    de_tahifi_debug.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_lists-doc.h: de_tahifi_lists.stamp
de_tahifi_lists-doc.md: de_tahifi_lists.stamp
de_tahifi_lists.c: de_tahifi_lists.stamp
de_tahifi_lists.h: de_tahifi_lists.stamp
de_tahifi_lists.stamp: $(DBUS_IFACES)/de_tahifi_lists.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_lists --c-namespace tdbus_lists --interface-prefix de.tahifi.Lists. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_lists-doc.md -H de_tahifi_lists-doc.h -c tdbus_lists -s de.tahifi.Lists. -n "$(PACKAGE_NAME) (source-independent content browsing)"
	touch $@

de_tahifi_artcache-doc.h: de_tahifi_artcache.stamp
de_tahifi_artcache-doc.md: de_tahifi_artcache.stamp
de_tahifi_artcache.c: de_tahifi_artcache.stamp
de_tahifi_artcache.h: de_tahifi_artcache.stamp
de_tahifi_artcache.stamp: $(DBUS_IFACES)/de_tahifi_artcache.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_artcache --c-namespace tdbus_artcache --interface-prefix de.tahifi.ArtCache. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_artcache-doc.md -H de_tahifi_artcache-doc.h -c tdbus_artcache -s de.tahifi.ArtCache. -n "$(PACKAGE_NAME) (cover art)"
	touch $@

de_tahifi_errors-doc.h: de_tahifi_errors.stamp
de_tahifi_errors-doc.md: de_tahifi_errors.stamp
de_tahifi_errors.c: de_tahifi_errors.stamp
de_tahifi_errors.h: de_tahifi_errors.stamp
de_tahifi_errors.stamp: $(DBUS_IFACES)/de_tahifi_errors.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_errors --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_errors-doc.md -H de_tahifi_errors-doc.h -c tdbus -s de.tahifi.. -n "$(PACKAGE_NAME) (cover art)"
	touch $@

de_tahifi_debug-doc.h: de_tahifi_debug.stamp
de_tahifi_debug-doc.md: de_tahifi_debug.stamp
de_tahifi_debug.c: de_tahifi_debug.stamp
de_tahifi_debug.h: de_tahifi_debug.stamp
de_tahifi_debug.stamp: $(DBUS_IFACES)/de_tahifi_debug.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_debug --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_debug-doc.md -H de_tahifi_debug-doc.h -c tdbus_debug -s de.tahifi.Debug. -n "Introspection"
	touch $@
