# List broker daemons

## Copyright and contact

The T+A List broker daemons (T+A List Brokers) are released under the terms of
the GNU General Public License version 2 or (at your option) any later version.
See file <tt>COPYING</tt> for licensing terms of the GNU General Public License
version 2, or <tt>COPYING.GPLv3</tt> for licensing terms of the GNU General
Public License version 3.

Contact:

    T+A elektroakustik GmbH & Co. KG
    Planckstrasse 11
    32052 Herford
    Germany

## Available list broker implementations

### Local file system

This implementation relies heavily on another project named mounTA, the T+A
Mount Daemon. The list broker reacts to signals from the mount daemon and
enables the user to browse the contents of all partitions that could be
mounted.

### UPnP AV

Browsing of UPnP AV directories, requires dLeyna for the UPnP part. This list
broker is basically a caching adapter between dLeyna's D-Bus API and our own
list browsing D-Bus API.
