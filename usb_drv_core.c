#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "usb_drv_hid.h"
#include "usb_drv_char.h"

/*******************************************************************************
* HELPER FUNCS
*******************************************************************************/
char *InterfaceClass_to_string(int class)
{
	char *s;

	switch (class) {
	case USB_CLASS_PER_INTERFACE:
		s = "PER_INTERFACE"; break;
	case USB_CLASS_AUDIO:
		s = "AUDIO"; break;
	case USB_CLASS_COMM:
		s = "COMM"; break;
	case USB_CLASS_HID:
		s = "HID"; break;
	case USB_CLASS_PHYSICAL:
		s = "PHYSICAL"; break;
	case USB_CLASS_STILL_IMAGE:
		s = "STILL_IMAGE"; break;
	case USB_CLASS_PRINTER:
		s = "PRINTER"; break;
	case USB_CLASS_MASS_STORAGE:
		s = "MASS_STORAGE"; break;
	case USB_CLASS_HUB:
		s = "HUB"; break;
	case USB_CLASS_CDC_DATA:
		s = "CDC_DATA"; break;
	case USB_CLASS_CSCID:
		s = "CSCID"; break;
	case USB_CLASS_CONTENT_SEC:
		s = "CONTENT_SEC"; break;
	case USB_CLASS_VIDEO:
		s = "VIDEO"; break;
	case USB_CLASS_WIRELESS_CONTROLLER:
		s = "WIRELESS_CONTROLLER"; break;
	case USB_CLASS_MISC:
		s = "MISC"; break;
	case USB_CLASS_APP_SPEC:
		s = "APP_SPEC"; break;
	case USB_CLASS_VENDOR_SPEC:
		s = "VENDOR_SPEC"; break;
	default:
		s = "UNKNOWN"; break;
	}

	return s;
}

char *endpointtype_to_string(int attr)
{
	char *s;

	switch (attr) {
	case USB_ENDPOINT_XFER_CONTROL:
		s = "CONTROL"; break;
	case USB_ENDPOINT_XFER_ISOC:
		s = "ISOC"; break;
	case USB_ENDPOINT_XFER_BULK:
		s = "BULK"; break;
	case USB_ENDPOINT_XFER_INT:
		s = "INT"; break;
	default:
		s = "UNKNOWN"; break;
	}

	return s;
}

bool endpointaddr_is_input(int addr)
{
	return (addr & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN;
}

/*******************************************************************************
* USB DRV
*******************************************************************************/
static int pcm_probe(struct usb_interface *iface, const struct usb_device_id *id)
{
	int ret = 0;
	int i, j;
	struct usb_device *device;
	struct usb_host_interface *iface_desc;

	device = interface_to_usbdev(iface);
	iface_desc = iface->cur_altsetting;

	/* print a bunch of interface info */
	pr_alert("====================== CALLED PROBE! ======================\n");
	pr_alert("Iface belongs to device num %d, at path %s:\n\tproduct=%s\n\tmanufacturer=%s\n\tserial=%s\n",
			 device->devnum, device->devpath, device->product, device->manufacturer, device->serial);
	pr_alert("ID->num_altsetting: %02X\n", iface->num_altsetting);

	for (i = 0; i < iface->num_altsetting; i++) {
		struct usb_host_interface *iface_desc_iter;

		iface_desc_iter = &iface->altsetting[i];
		pr_alert("\tIface %d now probed: (%04X:%04X)\n",
				 iface_desc_iter->desc.bInterfaceNumber,
				 id->idVendor, id->idProduct);
		pr_alert("\tIface->bInterfaceClass: %02X (%s)\n",
				 iface_desc_iter->desc.bInterfaceClass,
				 InterfaceClass_to_string(iface_desc_iter->desc.bInterfaceClass));
		pr_alert("\tIface->bNumEndpoints: %02X\n",
				 iface_desc_iter->desc.bNumEndpoints);

		/* traverse endpoints, ctrl endpoint is not explicitly listed */
		for (j = 0; j < iface_desc_iter->desc.bNumEndpoints; j++) {
			struct usb_endpoint_descriptor *endpoint_iter;

			endpoint_iter = &iface_desc_iter->endpoint[j].desc;
			pr_alert("\t\tED[%d]->bEndpointAddress: 0x%02X (Direction: %s)\n",
					 j, endpoint_iter->bEndpointAddress,
					 endpointaddr_is_input(endpoint_iter->bEndpointAddress)?"in":"out");
			pr_alert("\t\tED[%d]->bmAttributes: 0x%02X (xfertype: %s)\n",
					 j, endpoint_iter->bmAttributes,
					 endpointtype_to_string(endpoint_iter->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
			pr_alert("\t\tED[%d]->wMaxPacketSize: %d\n",
					 j, endpoint_iter->wMaxPacketSize);
		}
	}

	/* for now, only consider the HID iface (it only has one altsetting) */
	if (iface_desc->desc.bInterfaceClass == USB_CLASS_HID) {
		ret = pcm_hid_probe(iface);
		if (ret)
			goto out;

		ret = pcm_char_probe(iface);
		if (ret)
			goto out;
	}

out:
	return ret;
}

static void pcm_disconnect(struct usb_interface *iface)
{
	pr_alert("CALLED DISCONNECT!\n");

	pcm_hid_remove(iface);
	pcm_char_remove(iface);
}

/****************************** driver structures *****************************/
static struct usb_device_id pcm_id_table[] = {
	/* USB_DEVICE(vend, prod) */
	{ USB_DEVICE(0x08bb, 0x2704) },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(usb, pcm_id_table);

static struct usb_driver pcm_driver = {
	.name = "my-pcm2704-drv",
	.probe = pcm_probe,
	.disconnect = pcm_disconnect,
	.id_table = pcm_id_table,
};

module_usb_driver(pcm_driver);

/*******************************************************************************
* MODULE INFORMATION
*******************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marco Hartmann");
MODULE_DESCRIPTION("USB driver");
MODULE_VERSION(MOD_VER);
