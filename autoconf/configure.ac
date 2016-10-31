#################### Autoconf configure.ac ####################
# Process this file with autoconf to produce a configure script.
AC_REVISION([Revision: 0.01])

# we need Autoconf 2.69+ for proper aarch64 detection
AC_PREREQ([2.69])
AC_INIT([sunxi-tools], [1.4],
	[https://github.com/linux-sunxi/sunxi-tools/issues],
	[sunxi-tarball], [https://github.com/linux-sunxi/sunxi-tools])

AC_CONFIG_SRCDIR([fel-sdboot.S])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([autoconf])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
#AC_PROG_LN_S
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_OUTPUT
