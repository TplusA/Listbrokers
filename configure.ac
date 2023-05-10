dnl Copyright (C) 2015--2023  T+A elektroakustik GmbH & Co. KG
dnl
dnl This file is part of T+A List Brokers.
dnl
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License
dnl as published by the Free Software Foundation; either version 2
dnl of the License, or (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
dnl MA  02110-1301, USA.

AC_PREREQ([2.69])
AC_INIT([Listbrokers], [5.13], [r.tiemann@ta-hifi.com])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([1.11 -Wall foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/common/dbus_lists_iface.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS()

AC_ARG_VAR([CWARNINGS], [strict set of C compiler warnings])
AC_ARG_VAR([CXXWARNINGS], [strict set of C++ compiler warnings])
AC_ARG_VAR([CRELAXEDWARNINGS], [options for relaxing some C compiler warnings for various files])
AC_ARG_VAR([CXXRELAXEDWARNINGS], [options for relaxing some C++ compiler warnings for various files])

AC_ARG_ENABLE([valgrind],
              [AS_HELP_STRING([--disable-valgrind],
                              [disable an extra run of unit tests with Valgrind on ``make check''])],
              [],
              [enable_valgrind=yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_AR
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG([0.26])
AC_CHECK_PROGS([MARKDOWN], [markdown])
AC_CHECK_PROGS([DOXYGEN], [doxygen])
AC_CHECK_PROGS([GDBUS_CODEGEN], [gdbus-codegen])
AC_CHECK_PROGS([XSLTPROC], [xsltproc])
AC_CHECK_PROGS([VALGRIND], [valgrind])
AC_CHECK_PROGS([AUTOREVISION], [autorevision])

m4_ifdef([AC_CHECK_CUTTER],
[
AC_CHECK_CUTTER
AC_CHECK_CPPCUTTER
AC_CHECK_GCUTTER
AC_CHECK_COVERAGE
],
[ac_cv_use_cutter=no])

# Checks for libraries.
PKG_CHECK_MODULES([LISTBROKER_DEPENDENCIES], [gmodule-2.0 gio-2.0 gio-unix-2.0 gthread-2.0])
PKG_CHECK_MODULES([LISTBROKER_USB_DEPENDENCIES], [curlpp])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

m4_ifdef([AC_CHECK_CUTTER],
[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$pkg_cv_CUTTER_CFLAGS"
AC_CHECK_HEADER([cutter.h], [], [ac_cv_use_cutter=no])
CPPFLAGS="$save_CPPFLAGS"

AC_LANG_PUSH([C++])
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$pkg_cv_CPPCUTTER_CFLAGS"
AC_CHECK_HEADER([cppcutter.h], [], [ac_cv_use_cutter=no])
CPPFLAGS="$save_CPPFLAGS"
AC_LANG_POP([C++])
])

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_17([noext])
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.

AM_CONDITIONAL([WITH_CUTTER], [test "x$ac_cv_use_cutter" = "xyes"])
AM_CONDITIONAL([WITH_VALGRIND], [test "x$enable_valgrind" = "xyes"])
AM_CONDITIONAL([WITH_MARKDOWN], [test "x$ac_cv_prog_MARKDOWN" != "x"])

AC_SUBST([DBUS_IFACES], ["\$(top_srcdir)/dbus_interfaces"])

AC_CONFIG_FILES([Makefile src/common/Makefile src/usb/Makefile src/dlna/Makefile tests/Makefile dist/Makefile Doxyfile])
AC_CONFIG_FILES([versioninfo.cache])
AC_CONFIG_FILES([tests/run_test.sh], [chmod +x tests/run_test.sh])
AC_CONFIG_FILES([tests/valgrind.sh], [chmod +x tests/valgrind.sh])
AC_CONFIG_FILES([tests/gdb_run_test.sh], [chmod +x tests/gdb_run_test.sh])
AC_OUTPUT
