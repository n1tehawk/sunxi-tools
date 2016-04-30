/*
 * Copyright (C) 2012  Henrik Nordstrom <henrik@henriknordstrom.net>
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

/**********************************************************************
 * USB library and helper functions for the FEL utility
 **********************************************************************/
#include "fel_usblib.h"

#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "portable_endian.h"

static int timeout = 10000; /* 10 seconds */

/* This is our (private/opaque) data type that we'll pass as "handle" */
struct _felusb_handle {
	libusb_device_handle *usb;
	int endpoint_out, endpoint_in;
	bool iface_detached;
};

struct aw_usb_request {
	char signature[8];
	uint32_t length;
	uint32_t unknown1;	/* 0x0c000000 */
	uint16_t request;
	uint32_t length2;	/* Same as length */
	char pad[10];
} __attribute__((packed));

static const int AW_USB_READ    = 0x11;
static const int AW_USB_WRITE   = 0x12;

struct aw_fel_request {
	uint32_t request;
	uint32_t address;
	uint32_t length;
	uint32_t pad;
};

/* FEL request types */
static const int AW_FEL_VERSION = 0x001;
static const int AW_FEL_1_WRITE = 0x101;
static const int AW_FEL_1_EXEC  = 0x102;
static const int AW_FEL_1_READ  = 0x103;


/* a helper function to report libusb errors */
static void usb_error(int rc, const char *caption, int exitcode)
{
	if (caption)
		pr_error("%s ", caption);

#if defined(LIBUSBX_API_VERSION) && (LIBUSBX_API_VERSION >= 0x01000102)
	pr_error("ERROR %d: %s\n", rc, libusb_strerror(rc));
#else
	/* assume that libusb_strerror() is missing in the libusb API */
	pr_error("ERROR %d\n", rc);
#endif

	if (exitcode != 0)
		exit(exitcode);
}

/* general functions */

static int felusb_get_endpoint(felusb_handle *handle)
{
	struct libusb_device *dev = libusb_get_device(handle->usb);
	struct libusb_config_descriptor *config;
	int if_idx, set_idx, ep_idx, ret;

	ret = libusb_get_active_config_descriptor(dev, &config);
	if (ret)
		return ret;

	for (if_idx = 0; if_idx < config->bNumInterfaces; if_idx++) {
		const struct libusb_interface *iface = config->interface + if_idx;

		for (set_idx = 0; set_idx < iface->num_altsetting; set_idx++) {
			const struct libusb_interface_descriptor *setting =
				iface->altsetting + set_idx;

			for (ep_idx = 0; ep_idx < setting->bNumEndpoints; ep_idx++) {
				const struct libusb_endpoint_descriptor *ep =
					setting->endpoint + ep_idx;

				/* Test for bulk transfer endpoint */
				if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
						LIBUSB_TRANSFER_TYPE_BULK)
					continue;

				if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
						LIBUSB_ENDPOINT_IN)
					handle->endpoint_in = ep->bEndpointAddress;
				else
					handle->endpoint_out = ep->bEndpointAddress;
			}
		}
	}

	libusb_free_config_descriptor(config);

	return LIBUSB_SUCCESS;
}

static void felusb_claim(felusb_handle *handle)
{
	int rc = libusb_claim_interface(handle->usb, 0);
#if defined(__linux__)
	if (rc != LIBUSB_SUCCESS) {
		libusb_detach_kernel_driver(handle->usb, 0);
		handle->iface_detached = true;
		rc = libusb_claim_interface(handle->usb, 0);
	}
#endif
	if (rc)
		usb_error(rc, "libusb_claim_interface()", 1);

	rc = felusb_get_endpoint(handle);
	if (rc)
		usb_error(rc, "FAILED to get FEL mode endpoint addresses!", 1);
}

static void felusb_release(felusb_handle *handle)
{
	libusb_release_interface(handle->usb, 0);
#if defined(__linux__)
	if (handle->iface_detached)
		libusb_attach_kernel_driver(handle->usb, 0);
#endif
}

/* open libusb handle to desired FEL device */
felusb_handle *open_fel_device(int busnum, int devnum,
			       uint16_t vendor_id, uint16_t product_id)
{
	felusb_handle *result = calloc(1, sizeof(felusb_handle));
	if (!result) {
		pr_error("FAILED to allocate felusb_handle memory.\n");
		exit(1);
	}

	if (busnum < 0 || devnum < 0) {
		/* With the default values (busnum -1, devnum -1) we don't care
		 * for a specific USB device; so let libusb open the first
		 * device that matches VID/PID.
		 */
		result->usb = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
		if (!result->usb) {
			switch (errno) {
			case EACCES:
				pr_error("ERROR: You don't have permission to access Allwinner USB FEL device\n");
				break;
			default:
				pr_error("ERROR: Allwinner USB FEL device not found!\n");
			}
			exit(1);
		}
	} else {
		/* look for specific bus and device number */
		libusb_device **list;
		ssize_t rc, i;

		rc = libusb_get_device_list(NULL, &list);
		if (rc < 0)
			usb_error(rc, "libusb_get_device_list()", 1);
		for (i = 0; i < rc; i++) {
			if (libusb_get_bus_number(list[i]) == busnum
			    && libusb_get_device_address(list[i]) == devnum) {
				/* bus:devnum matched */
				struct libusb_device_descriptor desc;
				libusb_get_device_descriptor(list[i], &desc);
				if (desc.idVendor != vendor_id
				    || desc.idProduct != product_id) {
					pr_error("ERROR: Bus %03d Device %03d not a FEL device "
						 "(expected %04x:%04x, got %04x:%04x)\n", busnum, devnum,
						 vendor_id, product_id, desc.idVendor, desc.idProduct);
					exit(1);
				}
				/* open handle to this specific device (incrementing its refcount) */
				rc = libusb_open(list[i], &result->usb);
				if (rc != 0)
					usb_error(rc, "libusb_open()", 1);
				break;
			}
		}
		libusb_free_device_list(list, true);

		if (!result->usb) {
			pr_error("ERROR: Bus %03d Device %03d not found in libusb device list\n",
				 busnum, devnum);
			exit(1);
		}
	}

	felusb_claim(result); /* claim interface, detect USB endpoints */

	return result;
}

void felusb_close(felusb_handle *handle)
{
	felusb_release(handle);
	libusb_close(handle->usb);
	free(handle); /* release memory allocated for struct */
}

void felusb_init(void)
{
	int rc = libusb_init(NULL);
	if (rc != 0)
		usb_error(rc, "libusb_init()", 1);
}

void felusb_done(felusb_handle *handle)
{
	if (handle)
		felusb_close(handle);
	libusb_exit(NULL);
}

/*
 * AW_USB_MAX_BULK_SEND and the timeout constant are related.
 * Both need to be selected in a way that transferring the maximum chunk size
 * with (SoC-specific) slow transfer speed won't time out.
 *
 * The 512 KiB here are chosen based on the assumption that we want a 10 seconds
 * timeout, and "slow" transfers take place at approx. 64 KiB/sec - so we can
 * expect the maximum chunk being transmitted within 8 seconds or less.
 */
static const int AW_USB_MAX_BULK_SEND = 512 * 1024; /* 512 KiB per bulk request */

static void usb_bulk_send(felusb_handle *handle, int ep, const void *data,
			  size_t length, bool progress)
{
	/*
	 * With no progress notifications, we'll use the maximum chunk size.
	 * Otherwise, it's useful to lower the size (have more chunks) to get
	 * more frequent status updates. 128 KiB per request seem suitable.
	 * (Worst case of "slow" transfers -> one update every two seconds.)
	 */
	size_t max_chunk = progress ? 128 * 1024 : AW_USB_MAX_BULK_SEND;

	size_t chunk;
	int rc, sent;
	while (length > 0) {
		chunk = length < max_chunk ? length : max_chunk;
		rc = libusb_bulk_transfer(handle->usb, ep, (void *)data, chunk, &sent, timeout);
		if (rc != 0)
			usb_error(rc, "usb_bulk_send()", 2);
		length -= sent;
		data += sent;

		if (progress)
			progress_update(sent); /* notification after each chunk */
	}
}

static void usb_bulk_recv(felusb_handle *handle, int ep, void *data, int length)
{
	int rc, recv;
	while (length > 0) {
		rc = libusb_bulk_transfer(handle->usb, ep, data, length, &recv, timeout);
		if (rc != 0)
			usb_error(rc, "usb_bulk_recv()", 2);
		length -= recv;
		data += recv;
	}
}

static void aw_send_usb_request(felusb_handle *handle, int type, int length)
{
	struct aw_usb_request req = {
		.signature = "AWUC",
		.request = htole16(type),
		.length = htole32(length),
		.unknown1 = htole32(0x0c000000)
	};
	req.length2 = req.length;
	usb_bulk_send(handle, handle->endpoint_out, &req, sizeof(req), false);
}

static void aw_read_usb_response(felusb_handle *handle)
{
	char buf[13];
	usb_bulk_recv(handle, handle->endpoint_in, buf, sizeof(buf));
	if (strcmp(buf, "AWUS") != 0) {
		pr_error("ERROR: aw_read_usb_response() signature mismatch!\n");
		exit(1);
	}
}

static void aw_usb_write(felusb_handle *handle, const void *data, size_t len,
		  bool progress)
{
	aw_send_usb_request(handle, AW_USB_WRITE, len);
	usb_bulk_send(handle, handle->endpoint_out, data, len, progress);
	aw_read_usb_response(handle);
}

static void aw_usb_read(felusb_handle *handle, const void *data, size_t len)
{
	aw_send_usb_request(handle, AW_USB_READ, len);
	usb_bulk_send(handle, handle->endpoint_in, data, len, false);
	aw_read_usb_response(handle);
}

static void aw_send_fel_request(felusb_handle *handle, int type,
				uint32_t addr, uint32_t length)
{
	struct aw_fel_request req = {
		.request = htole32(type),
		.address = htole32(addr),
		.length = htole32(length)
	};
	aw_usb_write(handle, &req, sizeof(req), false);
}

static void aw_read_fel_status(felusb_handle *handle)
{
	char buf[8];
	aw_usb_read(handle, buf, sizeof(buf));
}

/* AW_FEL_VERSION request */
void aw_fel_get_version(felusb_handle *handle, struct aw_fel_version *buf)
{
	aw_send_fel_request(handle, AW_FEL_VERSION, 0, 0);
	aw_usb_read(handle, buf, sizeof(*buf));
	aw_read_fel_status(handle);

	buf->soc_id = (le32toh(buf->soc_id) >> 8) & 0xFFFF;
	buf->unknown_0a = le32toh(buf->unknown_0a);
	buf->protocol = le32toh(buf->protocol);
	buf->scratchpad = le16toh(buf->scratchpad);
	buf->pad[0] = le32toh(buf->pad[0]);
	buf->pad[1] = le32toh(buf->pad[1]);
}

/* AW_FEL_1_READ request */
void aw_fel_read(felusb_handle *handle, uint32_t offset, void *buf, size_t len)
{
	aw_send_fel_request(handle, AW_FEL_1_READ, offset, len);
	aw_usb_read(handle, buf, len);
	aw_read_fel_status(handle);
}

/* AW_FEL_1_WRITE request */
void aw_fel_write(felusb_handle *handle, void *buf, uint32_t offset, size_t len)
{
	aw_send_fel_request(handle, AW_FEL_1_WRITE, offset, len);
	aw_usb_write(handle, buf, len, false);
	aw_read_fel_status(handle);
}

/* AW_FEL_1_EXEC request */
void aw_fel_execute(felusb_handle *handle, uint32_t offset)
{
	aw_send_fel_request(handle, AW_FEL_1_EXEC, offset, 0);
	aw_read_fel_status(handle);
}

/*
 * This function is a higher-level wrapper for the FEL write functionality.
 * Unlike aw_fel_write() above - which is intended for "internal" use - this
 * routine optionally allows progress callbacks.
 */
void aw_fel_write_buffer(felusb_handle *handle, void *buf, uint32_t offset,
			 size_t len, bool progress)
{
	aw_send_fel_request(handle, AW_FEL_1_WRITE, offset, len);
	aw_usb_write(handle, buf, len, progress);
	aw_read_fel_status(handle);
}
