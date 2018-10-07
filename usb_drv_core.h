#ifndef __USB_DRV_CORE_H
#define __USB_DRV_CORE_H

#include <linux/usb.h>
#include <linux/usb/input.h>

/*******************************************************************************
* DATA STRUCTURES
*******************************************************************************/
typedef struct {
	struct usb_device *usb_dev;
	char *name;
	/* interrupt endpoint descriptor of the HID interface */
	struct usb_endpoint_descriptor *hid_int_endpoint;
	/* buffer of the interrupt endpoint of the HID interface */
	char *hid_int_buffer;
	/* usb for the HID irq transfer */
	struct urb *irq_urb;
	/* Input for vol up/down buttons */
	struct input_dev *inp_dev;
} pcm_priv_t;

#endif /* __USB_DRV_CORE_H */
