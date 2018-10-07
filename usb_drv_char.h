#ifndef __USB_DRV_CHAR_H
#define __USB_DRV_CHAR_H

#include <linux/usb.h>

int pcm_char_probe(struct usb_interface *);
void pcm_char_remove(struct usb_interface *);

#endif /* __USB_DRV_CHAR_H */
