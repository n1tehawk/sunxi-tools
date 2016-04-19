/*
 * Copyright (C) 2016  Bernhard Nortmann <bernhard.nortmann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _SUNXI_TOOLS_FEL_USB_H
#define _SUNXI_TOOLS_FEL_USB_H

/* USB identifiers for Allwinner device in FEL mode */
#define AW_USB_VENDOR_ID	0x1F3A
#define AW_USB_PRODUCT_ID	0xEFE8

#include <stdbool.h>
#include <stdint.h>
#include "progress.h"

typedef struct _felusb_handle felusb_handle; /* opaque data type */

struct aw_fel_version {
	char signature[8];
	uint32_t soc_id;	/* 0x00162300 */
	uint32_t unknown_0a;	/* 1 */
	uint16_t protocol;	/* 1 */
	uint8_t  unknown_12;	/* 0x44 */
	uint8_t  unknown_13;	/* 0x08 */
	uint32_t scratchpad;	/* 0x7e00 */
	uint32_t pad[2];	/* unused */
} __attribute__((packed));

/* USB functions */

void felusb_init(void);
void felusb_done(felusb_handle *handle);

felusb_handle *open_fel_device(int busnum, int devnum,
			       uint16_t vendor_id, uint16_t product_id);
void felusb_close(felusb_handle *handle);

/* FEL functions */

void aw_fel_get_version(felusb_handle *handle, struct aw_fel_version *buf);
void aw_fel_read(felusb_handle *handle, uint32_t offset, void *buf, size_t len);
void aw_fel_write(felusb_handle *handle, void *buf, uint32_t offset, size_t len);
void aw_fel_write_buffer(felusb_handle *handle, void *buf, uint32_t offset,
			 size_t len, bool progress);
void aw_fel_execute(felusb_handle *handle, uint32_t offset);

#endif /* _SUNXI_TOOLS_FEL_USB_H */
