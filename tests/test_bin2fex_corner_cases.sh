#!/bin/bash
#
# === Test errors / corner cases of "bin2fex", improving on code coverage ===
#
BIN2FEX=../bin2fex
TESTFILE=sunxi-boards/sys_config/a10/a10-olinuxino-lime.bin

# have bin2fex explicitly read /dev/stdin, to force use of fexc.c's "read_all()"
cat ${TESTFILE} | ${BIN2FEX} /dev/stdin > /dev/null
