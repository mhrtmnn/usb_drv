#ifndef __USB_DRV_HID_H
#define __USB_DRV_HID_H

#include <linux/usb.h>

int pcm_hid_probe(struct usb_interface *);
void pcm_hid_remove(struct usb_interface *);

#endif /* __USB_DRV_HID_H */
