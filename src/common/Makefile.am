#
# Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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
    libdbus_lists_handlers.la \
    libdbus_lists_iface.la \
    libdbus_artcache_iface.la \
    libmd5.la \
    liblists_dbus.la \
    libartcache_dbus.la \
    libdebug_dbus.la \
    libinifile.la

liblru_la_SOURCES = \
    lru.cc lru.hh timebase.hh messages.h idtypes.hh \
    $(DBUS_IFACES)/de_tahifi_lists_context.h
liblru_la_CFLAGS = $(AM_CFLAGS)
liblru_la_CXXFLAGS = $(AM_CXXFLAGS)

libstrbourl_la_SOURCES = \
    strbo_url.hh strbo_url_schemes.hh strbo_url_helpers.hh \
    strbo_url_common.cc \
    messages.h
libstrbourl_la_CFLAGS = $(AM_CFLAGS)
libstrbourl_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_lists_handlers_la_SOURCES = \
    dbus_lists_handlers.cc dbus_lists_handlers.hh dbus_lists_handlers.h \
    listtree_glue.hh listtree_glue.cc \
    dbus_async_work.hh \
    urlstring.hh i18nstring.hh ranked_stream_links.hh md5.hh \
    listtree.hh
libdbus_lists_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_lists_handlers_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libdbus_lists_iface_la_SOURCES = \
    dbus_lists_iface.c dbus_lists_iface.h dbus_lists_iface_deep.h
libdbus_lists_iface_la_CFLAGS = $(CRELAXEDWARNINGS)
libdbus_lists_iface_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_artcache_iface_la_SOURCES = \
    dbus_artcache_iface.c dbus_artcache_iface.h dbus_artcache_iface_deep.h
libdbus_artcache_iface_la_CFLAGS = $(CRELAXEDWARNINGS)
libdbus_artcache_iface_la_CXXFLAGS = $(AM_CXXFLAGS)

liblisttree_la_SOURCES = \
    listtree_manager.cc listtree_manager.hh \
    cacheable.hh \
    dbus_async_work.cc dbus_async_work.hh \
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

nodist_liblists_dbus_la_SOURCES = lists_dbus.c lists_dbus.h
liblists_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

nodist_libartcache_dbus_la_SOURCES = artcache_dbus.c artcache_dbus.h
libartcache_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libdebug_dbus_la_SOURCES = \
    dbus_debug_levels.c dbus_debug_levels.h \
    messages_dbus.c messages_dbus.h
nodist_libdebug_dbus_la_SOURCES = debug_dbus.c debug_dbus.h
libdebug_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libinifile_la_SOURCES = inifile.c inifile.h
libinifile_la_CFLAGS = $(AM_CFLAGS)
libinifile_la_CXXFLAGS = $(AM_CXXFLAGS)

BUILT_SOURCES = \
    $(nodist_liblists_dbus_la_SOURCES) \
    de_tahifi_lists.h de_tahifi_lists.md \
    lists_dbus.stamp \
    $(nodist_libartcache_dbus_la_SOURCES) \
    de_tahifi_artcache.h de_tahifi_artcache.md \
    artcache_dbus.stamp \
    $(nodist_libdebug_dbus_la_SOURCES) \
    de_tahifi_debug.h de_tahifi_debug.md \
    debug_dbus.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_lists.h: lists_dbus.stamp
de_tahifi_lists.md: lists_dbus.stamp
lists_dbus.c: lists_dbus.stamp
lists_dbus.h: lists_dbus.stamp
lists_dbus.stamp: $(DBUS_IFACES)/de_tahifi_lists.xml
	$(GDBUS_CODEGEN) --generate-c-code=lists_dbus --c-namespace tdbus_lists --interface-prefix de.tahifi.Lists. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_lists.md -H de_tahifi_lists.h -c tdbus_lists -s de.tahifi.Lists. -n "$(PACKAGE_NAME) (source-independent content browsing)"
	touch $@

de_tahifi_artcache.h: artcache_dbus.stamp
de_tahifi_artcache.md: artcache_dbus.stamp
artcache_dbus.c: artcache_dbus.stamp
artcache_dbus.h: artcache_dbus.stamp
artcache_dbus.stamp: $(DBUS_IFACES)/de_tahifi_artcache.xml
	$(GDBUS_CODEGEN) --generate-c-code=artcache_dbus --c-namespace tdbus_artcache --interface-prefix de.tahifi.ArtCache. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_artcache.md -H de_tahifi_artcache.h -c tdbus_artcache -s de.tahifi.ArtCache. -n "$(PACKAGE_NAME) (cover art)"
	touch $@

de_tahifi_debug.h: debug_dbus.stamp
de_tahifi_debug.md: debug_dbus.stamp
debug_dbus.c: debug_dbus.stamp
debug_dbus.h: debug_dbus.stamp
debug_dbus.stamp: $(DBUS_IFACES)/de_tahifi_debug.xml
	$(GDBUS_CODEGEN) --generate-c-code=debug_dbus --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_debug.md -H de_tahifi_debug.h -c tdbus_debug -s de.tahifi.Debug. -n "Introspection"
	touch $@
