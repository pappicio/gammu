/**
 * libusb helper functions
 *
 * Part of Gammu project
 *
 * Copyright (C) 2009 Michal Čihař
 *
 * Licensed under GNU GPL version 2 or later
 */

#define WIN32_LEAN_AND_MEAN   // impedisce a windows.h di includere winsock.h
#include <winsock2.h>         // DEVE venire PRIMA di windows.h
#include <windows.h>
#include <ws2tcpip.h>         // se serve

#include <gammu-config.h>

#include <ctype.h>
#include <string.h>

#ifdef LIBUSB_FOUND
#include <libusb.h>
#endif
 


#include "../../gsmstate.h"
#if defined(GSM_ENABLE_DKU2PHONET) && defined(GSM_ENABLE_USBDEVICE)
#include "usb.h"

#include "../../gsmcomon.h"

#include "../../../libgammu/misc/string.h"

/**
 * Nokia USB vendor ID.
 */
#define NOKIA_VENDOR_ID 0x0421

/**
 * USB CDC Class ID.
 */
#define USB_CDC_CLASS           0x02
/**
 * USB CDC FBUS Subclass ID.
 */
#define USB_CDC_FBUS_SUBCLASS       0xfe

/**
 * Union of CDC descriptor.
 */
#include <stdint.h>

#ifdef _WIN32
#pragma pack(push, 1)
#endif

struct cdc_union_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bMasterInterface0;
    uint8_t bSlaveInterface0;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
;

struct cdc_extra_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
;

#ifdef _WIN32
#pragma pack(pop)
#endif

/* CDC header types (bDescriptorSubType) */
#define CDC_HEADER_TYPE         0x00
#define CDC_UNION_TYPE          0x06
#define CDC_FBUS_TYPE           0x15

/* Interface descriptor (bDescriptorType) */
#define USB_DT_CS_INTERFACE     0x24


GSM_Error GSM_USB_Error(GSM_StateMachine *s, enum libusb_error code)
{
	switch (code) {
		case LIBUSB_SUCCESS:
			smprintf(s, "Success (no error)\n");
			return ERR_NONE;

		case LIBUSB_ERROR_IO:
			smprintf(s, "Input/output error\n");
			return ERR_DEVICEWRITEERROR;

		case LIBUSB_ERROR_INVALID_PARAM:
			smprintf(s, "Invalid parameter\n");
			return ERR_BUG;

		case LIBUSB_ERROR_ACCESS:
			smprintf(s, "Access denied (insufficient permissions)\n");
			return ERR_DEVICENOPERMISSION;

		case LIBUSB_ERROR_NO_DEVICE:
			smprintf(s, "No such device (it may have been disconnected)\n");
			return ERR_DEVICENOTEXIST;

		case LIBUSB_ERROR_NOT_FOUND:
			smprintf(s, "Entity not found\n");
			return ERR_DEVICENOTEXIST;

		case LIBUSB_ERROR_BUSY:
			smprintf(s, "Resource busy\n");
			return ERR_DEVICEBUSY;

		case LIBUSB_ERROR_TIMEOUT:
			smprintf(s, "Operation timed out\n");
			return ERR_TIMEOUT;

		case LIBUSB_ERROR_OVERFLOW:
			smprintf(s, "Overflow\n");
			return ERR_BUG;

		case LIBUSB_ERROR_PIPE:
			smprintf(s, "Pipe error\n");
			return ERR_BUG;

		case LIBUSB_ERROR_INTERRUPTED:
			smprintf(s, "System call interrupted (perhaps due to signal)\n");
			return ERR_BUG;

		case LIBUSB_ERROR_NO_MEM:
			smprintf(s, "Insufficient memory\n");
			return ERR_MOREMEMORY;

		case LIBUSB_ERROR_NOT_SUPPORTED:
			smprintf(s, "Operation not supported or unimplemented on this platform\n");
			return ERR_NOTSUPPORTED;

		case LIBUSB_ERROR_OTHER:
			smprintf(s, "Other error\n");
			return ERR_UNKNOWN;

		default:
			smprintf(s, "Unknown error\n");
			return ERR_UNKNOWN;
	}
}

GSM_Error GSM_USB_ParseDevice(GSM_StateMachine *s, int *vendor, int *product, int *bus, int *deviceid, char **serial)
{
	char *endptr, *next;
	int num;

	*vendor = -1;
	*product = -1;
	*bus = -1;
	*deviceid = -1;
	*serial = NULL;

	if (s->CurrentConfig->Device[0] == 0) {
		return ERR_NONE;
	}

	if (strncasecmp(s->CurrentConfig->Device, "serial:", 7) == 0) {
		*serial = s->CurrentConfig->Device + 7;
	} else if (strncasecmp(s->CurrentConfig->Device, "serial :", 8) == 0) {
		*serial = s->CurrentConfig->Device + 8;
	}
	if (*serial != NULL) {
		while (isspace(**serial) && **serial != 0) {
			*serial = *serial + 1;
		}
		smprintf(s, "Will search for serial = %s\n", *serial);
		return ERR_NONE;
	}

	if (!isdigit(s->CurrentConfig->Device[0])) {
		return ERR_NONE;
	}

	num = strtol(s->CurrentConfig->Device, &endptr, 10);
	if (*endptr == 'x') {
		num = strtol(s->CurrentConfig->Device, &endptr, 16);
	}
	if (*endptr == 0) {
		*deviceid = num;
		smprintf(s, "Will search for deviceid = %d\n", *deviceid);
		return ERR_NONE;
	}
	if (*endptr == ':') {
		*vendor = num;
		next = endptr + 1;
		num = strtol(next, &endptr, 10);
		if (*endptr == 'x') {
			num = strtol(next, &endptr, 16);
		}
		*product = num;
		smprintf(s, "Will search for vendor = 0x%04x, deviceid = 0x%04x\n", *vendor, *product);
		if (*endptr == 0) {
			return ERR_NONE;
		} else {
			return ERR_UNKNOWN;
		}
	}
	if (*endptr == '.') {
		*bus = num;
		next = endptr + 1;
		num = strtol(next, &endptr, 10);
		if (*endptr == 'x') {
			num = strtol(next, &endptr, 16);
		}
		*deviceid = num;
		smprintf(s, "Will search for bus = %d, deviceid = %d\n", *bus, *deviceid);
		if (*endptr == 0) {
			return ERR_NONE;
		} else {
			return ERR_UNKNOWN;
		}
	}
	return ERR_UNKNOWN;
}

GSM_Error GSM_USB_Probe(GSM_StateMachine *s, GSM_USB_Match_Function matcher)
{
	libusb_device **devs;
	libusb_device *dev;
	GSM_Device_USBData *d = &s->Device.Data.USB;
	ssize_t cnt;
	int rc;
	int i = 0;
	struct libusb_device_descriptor desc;
	struct libusb_config_descriptor *config;
	GSM_Error error;
	int vendor = -1, product = -1, bus = -1, deviceid = -1;
	char *serial = NULL;
	char buffer[300];
	gboolean do_match;

	/* Device selection */
	GSM_USB_ParseDevice(s, &vendor, &product, &bus, &deviceid, &serial);
	do_match = (vendor != -1 || product != -1 || bus != -1 || deviceid != -1 || serial != NULL);

	cnt = libusb_get_device_list(d->context, &devs);
	if (cnt < 0) {
		smprintf(s, "Failed to list USB devices (%d)!\n", (int)cnt);
		return GSM_USB_Error(s, cnt);
	}

	/* Check all devices */
	i = 0;
	while ((dev = devs[i++]) != NULL) {
		rc = libusb_get_device_descriptor(dev, &desc);
		if (rc < 0) {
			smprintf(s, "Failed to get device descriptor (%d)!\n", rc);
			GSM_USB_Error(s, rc);
			continue;
		}

		smprintf(s, "Checking %04x:%04x (bus %d, device %d)\n",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

		if (do_match) {
			if (vendor != -1 && vendor != desc.idVendor) {
				smprintf(s, "Vendor does not match requested 0x%04x, ignoring\n", vendor);
				continue;
			}
			if (product != -1 && product != desc.idProduct) {
				smprintf(s, "Product does not match requested 0x%04x, ignoring\n", product);
				continue;
			}
			if (bus != -1 && bus != libusb_get_bus_number(dev)) {
				smprintf(s, "Bus does not match requested %d, ignoring\n", bus);
				continue;
			}
			if (deviceid != -1 && deviceid != libusb_get_device_address(dev)) {
				smprintf(s, "Device address does not match requested %d, ignoring\n", deviceid);
				continue;
			}
			if (serial != NULL && desc.iSerialNumber) {
				rc = libusb_open(dev, &d->handle);
				if (rc != 0) {
					smprintf(s, "Failed to read serial!\n");
				} else {
					libusb_get_string_descriptor_ascii(d->handle, desc.iSerialNumber, buffer, sizeof(buffer) - 1);
					smprintf(s, "Device serial: %s\n", buffer);
					libusb_close(d->handle);
					if (strcasecmp(buffer, serial) != 0) {
						smprintf(s, "Device serial does not match requested %s, ignoring\n", serial);
						continue;
					}
				}
			}
		}

		if (matcher(s, dev, &desc)) {
			break;
		}
	}

	if (dev == NULL) {
		error = ERR_DEVICENOTEXIST;
		goto done;
	}

	smprintf(s, "Trying to open device, config=%d, c_iface=%d, c_alt=%d, d_iface=%d, d_alt=%d\n",
		d->configuration, d->control_iface, d->control_altsetting, d->data_iface, d->data_altsetting);

	rc = libusb_open(dev, &d->handle);
	if (rc != 0) {
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	/* Unhook kernel driver */
	rc = libusb_get_active_config_descriptor(dev, &config);
	if (rc != 0) {
		smprintf(s, "Failed to get current device configuration!\n");
		libusb_close(d->handle);
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	/* Do we have to change configuration? */
	if (config->bConfigurationValue != d->configuration) {
		smprintf(s, "Will change configuration, unhooking all interfaces!\n");
		for (i = 0; i < config->bNumInterfaces; i++) {
			if (libusb_kernel_driver_active(d->handle, i) == 1) {
				smprintf(s, "Detaching kernel driver from interface %d\n", i);
				rc = libusb_detach_kernel_driver(d->handle, i);
				if (rc != 0) {
					smprintf(s, "Failed to detach kernel driver!\n");
					libusb_free_config_descriptor(config);
					libusb_close(d->handle);
					d->handle = NULL;
					error = GSM_USB_Error(s, rc);
					goto done;
				}
			}
		}


		smprintf(s, "Configuring USB device...\n");
		rc = libusb_set_configuration(d->handle, d->configuration);
		if (rc != 0) {
			smprintf(s, "Failed to set device configuration %d (%d)!\n", d->configuration, rc);
			libusb_free_config_descriptor(config);
			libusb_close(d->handle);
			d->handle = NULL;
			error = GSM_USB_Error(s, rc);
			goto done;
		}
	} else {
		smprintf(s, "Configuration change not required, unhooking only required interfaces!\n");

		if (libusb_kernel_driver_active(d->handle, d->control_iface) == 1) {
			smprintf(s, "Detaching kernel driver from interface %d\n", d->control_iface);
			rc = libusb_detach_kernel_driver(d->handle, d->control_iface);
			if (rc != 0) {
				smprintf(s, "Failed to detach kernel driver!\n");
				libusb_free_config_descriptor(config);
				libusb_close(d->handle);
				d->handle = NULL;
				error = GSM_USB_Error(s, rc);
				goto done;
			}
		}

		if (libusb_kernel_driver_active(d->handle, d->data_iface) == 1) {
			smprintf(s, "Detaching kernel driver from interface %d\n", d->data_iface);
			rc = libusb_detach_kernel_driver(d->handle, d->data_iface);
			if (rc != 0) {
				smprintf(s, "Failed to detach kernel driver!\n");
				libusb_free_config_descriptor(config);
				libusb_close(d->handle);
				d->handle = NULL;
				error = GSM_USB_Error(s, rc);
				goto done;
			}
		}
	}
	libusb_free_config_descriptor(config);

	smprintf(s, "Claiming USB control interface...\n");
	rc = libusb_claim_interface(d->handle, d->control_iface);
	if (rc != 0) {
		smprintf(s, "Failed to set claim control interface %d (%d)!\n", d->control_iface, rc);
		libusb_close(d->handle);
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	smprintf(s, "Configuring USB control interface...\n");
	rc = libusb_set_interface_alt_setting(d->handle, d->control_iface, d->control_altsetting);
	if (rc != 0) {
		smprintf(s, "Failed to set control alt setting %d (%d)!\n", d->control_altsetting, rc);
		libusb_close(d->handle);
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	smprintf(s, "Claiming USB data interface...\n");
	rc = libusb_claim_interface(d->handle, d->data_iface);
	if (rc != 0) {
		smprintf(s, "Failed to set claim data interface %d (%d)!\n", d->data_iface, rc);
		libusb_close(d->handle);
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	smprintf(s, "Configuring USB data interface...\n");
	rc = libusb_set_interface_alt_setting(d->handle, d->data_iface, d->data_altsetting);
	if (rc != 0) {
		smprintf(s, "Failed to set data alt setting %d (%d)!\n", d->data_altsetting, rc);
		libusb_close(d->handle);
		d->handle = NULL;
		error = GSM_USB_Error(s, rc);
		goto done;
	}

	smprintf(s, "Connected!\n");
	error = ERR_NONE;

done:

	return error;
}

GSM_Error GSM_USB_Init(GSM_StateMachine *s)
{
	GSM_Device_USBData *d = &s->Device.Data.USB;
	int rc;

	d->handle = NULL;

	rc = libusb_init(&d->context);
	if (rc != 0) {
		d->context = NULL;
		smprintf(s, "Failed to init libusb (%d)!\n", rc);
		return GSM_USB_Error(s, rc);
	}

	return ERR_NONE;
}

GSM_Error GSM_USB_Terminate(GSM_StateMachine *s)
{
	GSM_Device_USBData *d = &s->Device.Data.USB;
	int rc;

	if (d->handle != NULL) {
		rc = libusb_set_interface_alt_setting(d->handle, d->data_iface, d->data_idlesetting);
		if (rc != 0) {
			smprintf(s, "Failed to set idle settings\n");
			return GSM_USB_Error(s, rc);
		}
		rc = libusb_release_interface(d->handle, d->control_iface);
		if (rc != 0) {
			smprintf(s, "Failed to release control interface\n");
			return GSM_USB_Error(s, rc);
		}
		rc = libusb_release_interface(d->handle, d->data_iface);
		if (rc != 0) {
			smprintf(s, "Failed to release data interface\n");
			return GSM_USB_Error(s, rc);
		}
		libusb_close(d->handle);
	}

	libusb_exit(d->context);

	d->handle = NULL;
	d->context = NULL;

	return ERR_NONE;
}

ssize_t GSM_USB_Read(GSM_StateMachine *s, void *buf, size_t nbytes)
{
	GSM_Device_USBData *d = &s->Device.Data.USB;
	int rc = LIBUSB_ERROR_TIMEOUT, ret = 0, repeat = 0;

	while (repeat < 10 && (rc == LIBUSB_ERROR_TIMEOUT || rc == LIBUSB_ERROR_INTERRUPTED || rc == LIBUSB_ERROR_OTHER || rc == LIBUSB_ERROR_NO_MEM)) {
		rc = libusb_bulk_transfer(d->handle, d->ep_read, buf, nbytes, &ret, 1000);
		/* This seems to be some strange failure on partial data transfer */
		if (rc == LIBUSB_ERROR_OTHER && ret != 0) {
			smprintf(s, "Other error while reading, but got some data\n");
			rc = 0;
			break;
		}
		if (rc == LIBUSB_ERROR_TIMEOUT && ret != 0) {
			smprintf(s, "Timeout while reading, but got some data\n");
			rc = 0;
			break;
		}
		if (rc != 0) {
			smprintf(s, "Failed to read from usb (%d)!\n", rc);
			GSM_USB_Error(s, rc);
		}
		repeat++;
		usleep(1000);
	}
	if (rc != 0) {
		return -1;
	}
	return ret;
}

ssize_t GSM_USB_Write(GSM_StateMachine *s, const void *buf, size_t nbytes)
{
	GSM_Device_USBData *d = &s->Device.Data.USB;
	int rc = LIBUSB_ERROR_TIMEOUT, ret = 0, repeat = 0;

	while (repeat < 10 && (rc == LIBUSB_ERROR_TIMEOUT || rc == LIBUSB_ERROR_INTERRUPTED || rc == LIBUSB_ERROR_OTHER || rc == LIBUSB_ERROR_NO_MEM)) {
		rc = libusb_bulk_transfer(d->handle, d->ep_write, (void *)buf, nbytes, &ret, 1000);
		/* This seems to be some strange failure on partial data transfer */
		if (rc == LIBUSB_ERROR_OTHER && ret != 0) {
			smprintf(s, "Other error while writing, but got some data\n");
			rc = 0;
			break;
		}
		if (rc == LIBUSB_ERROR_TIMEOUT && ret != 0) {
			smprintf(s, "Timeout while write, but some data were written\n");
			rc = 0;
			break;
		}
		if (rc != 0) {
			smprintf(s, "Failed to write to usb (%d)!\n", rc);
			GSM_USB_Error(s, rc);
		}
		repeat++;
		usleep(1000);
	}
	if (rc != 0) {
		return -1;
	}
	return ret;
}

/**
 * Find parameters for FBUS connection over USB.
 */
gboolean FBUSUSB_Match(GSM_StateMachine *s, libusb_device *dev, struct libusb_device_descriptor *desc)
{
	int c, i, a;
	int rc;
	struct libusb_config_descriptor *config;
	GSM_Device_USBData *d = &s->Device.Data.USB;
	const unsigned char *buffer;
	int buflen;
	struct cdc_extra_desc *extra_desc;
	struct cdc_union_desc *union_desc = NULL;
	const struct libusb_endpoint_descriptor *ep1, *ep2;

	/* We care only about Nokia */
	if (desc->idVendor != NOKIA_VENDOR_ID) return FALSE;

	/* Find configuration we want */
	for (c = 0; c < desc->bNumConfigurations; c++) {
		rc = libusb_get_config_descriptor(dev, c, &config);
		if (rc != 0) {
			GSM_USB_Error(s, rc);
			return FALSE;
		}
		/* Find interface we want */
		for (i = 0; i < config->bNumInterfaces; i++) {
			for (a = 0; a < config->interface[i].num_altsetting; a++) {
				/* We want only CDC FBUS settings */
				if (config->interface[i].altsetting[a].bInterfaceClass == USB_CDC_CLASS
						&& config->interface[i].altsetting[a].bInterfaceSubClass == USB_CDC_FBUS_SUBCLASS
					) {
					/* We have it */
					goto found_control;
				}
			}
		}
		libusb_free_config_descriptor(config);
	}
	return FALSE;
found_control:
	/* Remember configuration which is interesting */
	d->configuration = config->bConfigurationValue;

	/* Remember control interface */
	d->control_iface = config->interface[i].altsetting[a].bInterfaceNumber;
	d->control_altsetting = config->interface[i].altsetting[a].bAlternateSetting;

	/* Find out data interface */

	/* Process extra descriptors */
	buffer = config->interface[i].altsetting[a].extra;
	buflen = config->interface[i].altsetting[a].extra_length;

	/* Each element has length as first byte and type as second */
	while (buflen > 0) {
		extra_desc = (struct cdc_extra_desc *)buffer; /* Convenience */
		if (extra_desc->bDescriptorType != USB_DT_CS_INTERFACE) {
			smprintf(s, "Extra CDC header: %d\n", extra_desc->bDescriptorType);
			goto next_el;
		}

		switch (extra_desc->bDescriptorSubType) {
			case CDC_UNION_TYPE:
				union_desc = (struct cdc_union_desc *)buffer;
				break;
			case CDC_HEADER_TYPE:
			case CDC_FBUS_TYPE:
				/* We know these, but ignore them */
				break;
			default:
				smprintf(s, "Extra CDC subheader: %d\n", extra_desc->bDescriptorSubType);
				break;
		}
next_el:
		buflen -= extra_desc->bLength;
		buffer += extra_desc->bLength;
	}

	if (union_desc == NULL) {
		smprintf(s, "Failed to find data end points!\n");
		libusb_free_config_descriptor(config);
		return FALSE;
	}
	d->data_iface = union_desc->bSlaveInterface0;
	d->data_altsetting = -1;
	d->data_idlesetting = -1;

	/* Find out end points and settings from data_iface */
	for (i = 0; i < config->bNumInterfaces; i++) {
		for (a = 0; a < config->interface[i].num_altsetting; a++) {
			/* We want only data interface we found */
			if (config->interface[i].altsetting[a].bInterfaceNumber == d->data_iface) {
				/* We have it */
				if (config->interface[i].altsetting[a].bNumEndpoints == 2) {
					/* Active config */
					ep1 = &(config->interface[i].altsetting[a].endpoint[0]);
					ep2 = &(config->interface[i].altsetting[a].endpoint[1]);
					if ((ep1->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK ||
						(ep2->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK) {
						/* We want only bulk transfer */
						continue;
					}
					if ((ep1->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN &&
						(ep2->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
						d->ep_read = ep1->bEndpointAddress;
						d->ep_write = ep2->bEndpointAddress;
					} else  if ((ep2->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN &&
						(ep1->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
						d->ep_read = ep2->bEndpointAddress;
						d->ep_write = ep1->bEndpointAddress;
					} else {
						continue;
					}
					d->data_altsetting = config->interface[i].altsetting[a].bAlternateSetting;
				} else if (config->interface[i].altsetting[a].bNumEndpoints == 0) {
					/* Idle config */
					d->data_idlesetting = config->interface[i].altsetting[a].bAlternateSetting;
				}
			}
		}
	}
	if (d->data_altsetting == -1 || d->data_idlesetting == -1) {
		smprintf(s, "Failed to find data interface (%d)\n", d->data_iface);
		libusb_free_config_descriptor(config);
		return FALSE;
	}

	/* Free config descriptor */
	libusb_free_config_descriptor(config);
	return TRUE;
}

GSM_Error FBUSUSB_Open(GSM_StateMachine *s)
{
	GSM_Error error;

	error = GSM_USB_Init(s);
	if (error != ERR_NONE) return error;

	error = GSM_USB_Probe(s, FBUSUSB_Match);
	if (error != ERR_NONE) return error;

	return ERR_NONE;
}


GSM_Device_Functions FBUSUSBDevice = {
    	FBUSUSB_Open,
    	GSM_USB_Terminate,
	NONEFUNCTION,
	NONEFUNCTION,
	NONEFUNCTION,
    	GSM_USB_Read,
    	GSM_USB_Write
};
#endif

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */
