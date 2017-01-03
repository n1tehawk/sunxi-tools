# If not specified explicitly: try to guess a suitable ARM toolchain prefix
PATH_DIRS := $(shell echo $$PATH | sed -e 's/:/ /g')
TOOLCHAIN ?= $(shell find $(PATH_DIRS) -executable -name 'arm*-gcc' -printf '%f\t' | cut -f 1 | sed -e 's/-gcc/-/')

#
# build "preprocessed" .h files for inclusion of ARM scratch code
#
AWK_O_TO_H := awk -f objdump_to_h.awk
SPL_THUNK := fel-to-spl-thunk.S
SID_READ := sid_read_root.S

all: $(SPL_THUNK) $(SID_READ)
	$(TOOLCHAIN)as -o $(subst .S,.o,$(SID_READ)) $(SID_READ)
	$(TOOLCHAIN)objdump -d $(subst .S,.o,$(SID_READ)) | $(AWK_O_TO_H) > $(subst .S,.h,$(SID_READ))
# Note: The SPL thunk requires a different output format. The "style" variable
# for awk controls this, and causes the htole32() conversion to be omitted.
	$(TOOLCHAIN)as -o $(subst .S,.o,$(SPL_THUNK)) $(SPL_THUNK)
	$(TOOLCHAIN)objdump -d $(subst .S,.o,$(SPL_THUNK)) | $(AWK_O_TO_H) -v style=old > $(subst .S,.h,$(SPL_THUNK))
# clean up object files afterwards
	rm -f *.o
