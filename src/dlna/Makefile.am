#
# Copyright (C) 2015--2023  T+A elektroakustik GmbH & Co. KG
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

AM_CPPFLAGS = -I$(srcdir)/../common
AM_CPPFLAGS += -I../common
AM_CPPFLAGS += -I$(DBUS_IFACES)
AM_CPPFLAGS += $(LISTBROKER_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

bin_PROGRAMS = strbo_lb_upnp

strbo_lb_upnp_SOURCES = \
    strbo_lb_upnp.cc \
    dbus_upnp_iface.cc \
    dbus_upnp_iface.hh \
    dbus_upnp_iface_deep.h \
    periodic_rescan.cc \
    periodic_rescan.hh \
    ../common/main.cc \
    ../common/main.hh \
    ../common/lru.hh \
    ../common/cachecontrol.hh \
    ../common/cachecontrol.cc \
    ../common/timebase.hh \
    ../common/idtypes.hh \
    ../common/lists.hh \
    ../common/lists_base.hh \
    ../common/enterchild_template.hh \
    ../common/enterchild_glue.hh \
    ../common/dbus_common.h \
    ../common/dbus_common.c \
    ../common/dbus_async_work.hh \
    ../common/messages.h \
    ../common/messages.c \
    ../common/messages_glib.h \
    ../common/messages_glib.c \
    ../common/backtrace.h \
    ../common/backtrace.c \
    ../common/os.h \
    ../common/os.c \
    ../common/urlstring.cc \
    ../common/urlstring.hh \
    ../common/i18nstring.hh \
    ../common/md5.hh \
    ../common/gerrorwrapper.hh \
    $(DBUS_IFACES)/de_tahifi_lists_errors.h \
    $(DBUS_IFACES)/de_tahifi_lists_errors.hh \
    $(DBUS_IFACES)/de_tahifi_lists_context.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.hh

noinst_LTLIBRARIES = \
    libupnp_list.la \
    libdbus_upnp_handlers.la \
    libdbus_upnp_helpers.la \
    libupnp_dleynaserver_dbus.la \
    libupnp_media_dbus.la

strbo_lb_upnp_CFLAGS = $(CRELAXEDWARNINGS)
strbo_lb_upnp_CXXFLAGS = $(CXXRELAXEDWARNINGS)

strbo_lb_upnp_LDADD = \
    $(noinst_LTLIBRARIES) \
    ../common/liblru.la \
    ../common/libcacheable.la \
    ../common/libdbus_lists_handlers.la \
    ../common/libdbus_asyncwork.la \
    ../common/liblisttree.la \
    ../common/libdebug_dbus.la \
    ../common/liberrors_dbus.la \
    ../common/liblists_dbus.la \
    ../common/libdbus_lists_iface.la \
    ../common/libdbus_artcache_iface.la \
    ../common/libartcache_dbus.la \
    ../common/libmd5.la \
    $(LISTBROKER_DEPENDENCIES_LIBS)

libupnp_list_la_SOURCES = \
    upnp_list.cc upnp_list.hh \
    upnp_listtree.cc upnp_listtree.hh \
    servers_lost_and_found.hh \
    ../common/listtree.hh \
    ../common/urlstring.hh \
    ../common/md5.hh \
    ../common/dbus_async_work.hh
libupnp_list_la_CFLAGS = $(AM_CFLAGS)
libupnp_list_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libdbus_upnp_handlers_la_SOURCES = dbus_upnp_handlers.cc dbus_upnp_handlers.hh
libdbus_upnp_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_upnp_handlers_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libdbus_upnp_helpers_la_SOURCES = \
    dbus_upnp_helpers.cc dbus_upnp_helpers.hh \
    dbus_upnp_list_filler.cc dbus_upnp_list_filler.hh dbus_upnp_list_filler_helpers.hh
libdbus_upnp_helpers_la_CFLAGS = $(CRELAXEDWARNINGS)
libdbus_upnp_helpers_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

nodist_libupnp_dleynaserver_dbus_la_SOURCES = com_intel_dleynaserver.c com_intel_dleynaserver.h
libupnp_dleynaserver_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

nodist_libupnp_media_dbus_la_SOURCES = org_gnome_upnp.c org_gnome_upnp.h
libupnp_media_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

DBUS_IFACE_PAGES = \
    com_intel_dleynaserver-doc.mdp \
    org_gnome_upnp-doc.mdp

BUILT_SOURCES = \
    all_interfaces_toc.md \
    all_interfaces.md \
    all_interfaces.stamp \
    $(DBUS_IFACE_PAGES) \
    $(nodist_libupnp_dleynaserver_dbus_la_SOURCES) \
    com_intel_dleynaserver-doc.h \
    com_intel_dleynaserver.stamp \
    $(nodist_libupnp_media_dbus_la_SOURCES) \
    org_gnome_upnp-doc.h \
    org_gnome_upnp.stamp

CLEANFILES = $(BUILT_SOURCES)

com_intel_dleynaserver-doc.h: com_intel_dleynaserver.stamp
com_intel_dleynaserver-doc.mdp: com_intel_dleynaserver.stamp
com_intel_dleynaserver.c: com_intel_dleynaserver.stamp
com_intel_dleynaserver.h: com_intel_dleynaserver.stamp
com_intel_dleynaserver.stamp: $(DBUS_IFACES)/com_intel_dleynaserver.xml
	$(GDBUS_CODEGEN) --generate-c-code=com_intel_dleynaserver --c-namespace tdbus_dleynaserver --interface-prefix com.intel.dLeynaServer. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o com_intel_dleynaserver-doc.mdp -H com_intel_dleynaserver-doc.h -c tdbus_dleynaserver -s com.intel.dLeynaServer. -n "$(PACKAGE_NAME) (DLNA server management)"
	touch $@

org_gnome_upnp-doc.h: org_gnome_upnp.stamp
org_gnome_upnp-doc.mdp: org_gnome_upnp.stamp
org_gnome_upnp.c: org_gnome_upnp.stamp
org_gnome_upnp.h: org_gnome_upnp.stamp
org_gnome_upnp.stamp: $(DBUS_IFACES)/org_gnome_upnp.xml
	$(GDBUS_CODEGEN) --generate-c-code=org_gnome_upnp --c-namespace tdbus_upnp --interface-prefix org.gnome.UPnP. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o org_gnome_upnp-doc.mdp -H org_gnome_upnp-doc.h -c tdbus_upnp -s org.gnome.UPnP. -n "$(PACKAGE_NAME) (DLNA content browsing)"
	touch $@

all_interfaces_toc.md: all_interfaces.stamp
all_interfaces.md: all_interfaces.stamp
all_interfaces.stamp: $(DBUS_IFACE_PAGES)
	$(DBUS_IFACES)/combine_iface_pages.sh -toc all_interfaces_toc.md -ifaces all_interfaces.md $(DBUS_IFACE_PAGES)
