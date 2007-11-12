# The autoconf initilization.
AC_INIT(fbctrl, 0.1.1, [mbroemme@plusserver.de], [fbctrl])

# Detect the canonical host and target build environment
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([no-dependencies])
AC_CONFIG_HEADERS([config.h:config.h.in])

# Define some common variables.
AC_DEFINE_UNQUOTED(AUTHOR, "Maik Broemme <$PACKAGE_BUGREPORT>", [Package author])
AC_DEFINE_UNQUOTED(RELEASE, "(switch-reloaded)", [Package release])

# Notices
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.6 $)

# Checking for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checking for x11 library.
PKG_CHECK_MODULES([XLIB], [x11])
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

# Creating files.
AC_OUTPUT([
Makefile
doc/Makefile
src/Makefile
])
