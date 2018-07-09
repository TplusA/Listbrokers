#
# Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
#
# This file is part of T+A List Brokers.
#
# T+A List Brokers is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 3 as
# published by the Free Software Foundation.
#
# T+A List Brokers is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with T+A List Brokers.  If not, see <http://www.gnu.org/licenses/>.
#

SUBDIRS = . src/common src/usb src/dlna dist tests

ACLOCAL_AMFLAGS = -I m4

if WITH_MARKDOWN
html_DATA = README.html
endif

EXTRA_DIST = \
    README.md \
    dbus_interfaces/extract_documentation.py \
    dbus_interfaces/com_intel_dleynaserver.xml \
    dbus_interfaces/de_tahifi_artcache.xml \
    dbus_interfaces/de_tahifi_lists.xml \
    dbus_interfaces/de_tahifi_configuration.xml \
    dbus_interfaces/de_tahifi_mounta.xml \
    dbus_interfaces/de_tahifi_debug.xml \
    dbus_interfaces/net_connman.xml \
    dbus_interfaces/org_gnome_upnp.xml \
    dbus_interfaces/de_tahifi_lists_errors.h \
    dbus_interfaces/de_tahifi_lists_errors.hh \
    dbus_interfaces/de_tahifi_lists_item_kinds.h \
    dbus_interfaces/de_tahifi_lists_item_kinds.hh

CLEANFILES = README.html

BUILT_SOURCES = versioninfo.h

CLEANFILES += $(BUILT_SOURCES)

versioninfo.h: versioninfo.cache
	(cd $(top_srcdir) && $(AUTOREVISION) -t h -o $(abs_top_builddir)/versioninfo.cache >$(abs_top_builddir)/$@)

.md.html:
	$(MARKDOWN) $< >$@
