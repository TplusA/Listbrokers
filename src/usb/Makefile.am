#
# Copyright (C) 2015--2017, 2019--2022  T+A elektroakustik GmbH & Co. KG
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
AM_CFLAGS += $(LISTBROKER_DEPENDENCIES_CFLAGS)
AM_CFLAGS += $(LISTBROKER_USB_DEPENDENCIES_CFLAGS)

AM_CXXFLAGS = $(CXXWARNINGS)
AM_CXXFLAGS += $(LISTBROKER_DEPENDENCIES_CFLAGS)
AM_CXXFLAGS += $(LISTBROKER_USB_DEPENDENCIES_CFLAGS)

bin_PROGRAMS = strbo_lb_usb

strbo_lb_usb_SOURCES = \
    strbo_lb_usb.cc \
    dbus_usb_iface.cc \
    dbus_usb_iface.hh \
    dbus_usb_iface_deep.h \
    usb_helpers.cc \
    usb_helpers.hh \
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
    $(DBUS_IFACES)/de_tahifi_lists_errors.h \
    $(DBUS_IFACES)/de_tahifi_lists_errors.hh \
    $(DBUS_IFACES)/de_tahifi_lists_context.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.hh

noinst_LTLIBRARIES = \
    libusb_list.la \
    libusb_strbourl.la \
    libdbus_mounta_handlers.la \
    libmounta_dbus.la

strbo_lb_usb_CFLAGS = $(CRELAXEDWARNINGS)
strbo_lb_usb_CXXFLAGS = $(CXXRELAXEDWARNINGS)

strbo_lb_usb_LDADD = \
    $(noinst_LTLIBRARIES) \
    ../common/liblru.la \
    ../common/libcacheable.la \
    ../common/libdbus_lists_handlers.la \
    ../common/libdbus_asyncwork.la \
    ../common/liblisttree.la \
    ../common/libdebug_dbus.la \
    ../common/liberrors_dbus.la \
    ../common/liblists_dbus.la \
    ../common/libstrbourl.la \
    ../common/libdbus_lists_iface.la \
    ../common/libdbus_artcache_iface.la \
    ../common/libartcache_dbus.la \
    ../common/libmd5.la \
    $(LISTBROKER_DEPENDENCIES_LIBS) \
    $(LISTBROKER_USB_DEPENDENCIES_LIBS)

libusb_list_la_SOURCES = \
    usb_list.cc usb_list.hh \
    usb_listtree.cc usb_listtree.hh \
    ../common/listtree.hh \
    ../common/md5.hh \
    ../common/dbus_async_work.hh
libusb_list_la_CFLAGS = $(AM_CFLAGS)
libusb_list_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libusb_strbourl_la_SOURCES = \
    strbo_url_usb.hh strbo_url_usb.cc \
    ../common/strbo_url.hh \
    ../common/strbo_url_schemes.hh \
    ../common/strbo_url_helpers.hh \
    ../common/messages.h
libusb_strbourl_la_CFLAGS = $(AM_CFLAGS)
libusb_strbourl_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_mounta_handlers_la_SOURCES = dbus_mounta_handlers.cc dbus_mounta_handlers.hh
libdbus_mounta_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_mounta_handlers_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

nodist_libmounta_dbus_la_SOURCES = de_tahifi_mounta.c de_tahifi_mounta.h
libmounta_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

BUILT_SOURCES = \
    $(nodist_libmounta_dbus_la_SOURCES) \
    de_tahifi_mounta-doc.h \
    de_tahifi_mounta-doc.md \
    de_tahifi_mounta.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_mounta-doc.h: de_tahifi_mounta.stamp
de_tahifi_mounta-doc.md: de_tahifi_mounta.stamp
de_tahifi_mounta.c: de_tahifi_mounta.stamp
de_tahifi_mounta.h: de_tahifi_mounta.stamp
de_tahifi_mounta.stamp: $(DBUS_IFACES)/de_tahifi_mounta.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_mounta --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_mounta-doc.md -H de_tahifi_mounta-doc.h -c tdbus -s de.tahifi. -n "$(PACKAGE_NAME) (T+A automounter)"
	touch $@
