AC_INIT([gentoo-systemd-initctl], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_PROG_CC
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd])
PKG_CHECK_VAR([systemdsystemunitdir], [systemd], [systemdsystemunitdir])
PKG_CHECK_VAR([systemdutildir], [systemd], [systemdutildir])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
