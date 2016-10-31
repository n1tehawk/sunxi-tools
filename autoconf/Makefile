#
# autoconf/Makefile to build new "configure" script and related files
#
# This Makefile is NOT INTENDED FOR END USERS.
# Instead package maintainers are expected to invoke it from a subdirectory,
# resulting in the output files being moved to ".." - preferable under
# version control (git). The Makefile relies on GNU autotools, namely the
# "autoconf" and "autoheader" utilities. (Note that these are not required
# for users, which will later only run the resulting "configure" script.)
#

.PHONY: new-config

new-config: configure config.h.in
	mv $^ ..

configure: configure.ac
	autoconf

config.h.in: configure.ac
	autoheader
