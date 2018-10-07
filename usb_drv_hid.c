#include <linux/usb/input.h>
#include <linux/nls.h>

#include "usb_drv_core.h"

/*******************************************************************************
* MACROS
*******************************************************************************/
#define VOL_MUTE_BUTTON BIT(0)
#define VOL_UP_BUTTON BIT(1)
#define VOL_DN_BUTTON BIT(2)

/*******************************************************************************
* INPUT DRV
*******************************************************************************/
static int manual_vendor_readout(struct usb_interface *iface)
{
	int ret;
	uint8_t buf[128], shrunken_buf[64];
	struct usb_device *device;

	device = interface_to_usbdev(iface);

	/**
	 * need to specify
	 * - target device
	 * - endpoint pipe to which msg should be delivered
	 * - USB message request value
	 * - USB message request type value
	 * - USB message value
	 * - USB message index value
	 * - msg buf
	 * - a timout for the blocking transfer.
	 *
	 * Returns number of read bytes.
	 */
	ret = usb_control_msg(device,
						  usb_rcvctrlpipe(device, 0 /*endpoint num*/), // usb_sndctrlpipe() for sending
						  USB_REQ_GET_DESCRIPTOR,
						  USB_DIR_IN,
						  (USB_DT_STRING << 8) + device->descriptor.iManufacturer,
						  device->string_langid,
						  buf,
						  sizeof(buf),
						  USB_CTRL_GET_TIMEOUT);
	if (ret < 0)
		goto usb_comm_err;

	/**
	 * Msg has following layout:
	 * | <MSG_LEN> | <DESCRIPTOR_TYPE> (| <MSG_BYTE> | <0x00>)^((MSG_LEN-2)/2) |
	 *
	 * Sanity check the descriptor type
	 */
	if (buf[1] != USB_DT_STRING)
		goto wrong_descriptor;

	/**
	 * Two uint8_t elements from buf form a single uint16_t ASCII character
	 * The high byte is unused (i.e. 0x00) so we need to truncate it,
	 * and copy each low byte into a new buffer.
	 * Size if array will halve, reserve last element for NULL termination.
	 */
	ret = utf16s_to_utf8s((wchar_t *) &buf[2], (ret - 2) / 2,
						  UTF16_LITTLE_ENDIAN, shrunken_buf, sizeof(buf)-1);
	shrunken_buf[ret] = 0;

	pr_alert("manual read of usb_string->iManufacturer='%s' (got %d characters)\n", shrunken_buf, ret);
	return 0;

/* error handling */
usb_comm_err:
	dev_err(&iface->dev, "Cannot communicate with usb device!\n");
	return -ENODATA;

wrong_descriptor:
	dev_err(&device->dev, "wrong descriptor type %02x for string %d (is: %d, should be %d)\n",
			buf[1], device->descriptor.iManufacturer, buf[1], USB_DT_STRING);
	return -ENODATA;
}

static int pcm_hid_inp_open(struct input_dev *dev)
{
	pcm_priv_t *pcm_priv = input_get_drvdata(dev);

	/* NullPtrExc if not set */
	pcm_priv->irq_urb->dev = pcm_priv->usb_dev;

	/**
	 * Submit transfer request. Request completion will be indicated later
	 * (asynchronously)by calling the completion handler.
	 */
	if (usb_submit_urb(pcm_priv->irq_urb, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void pcm_hid_inp_close(struct input_dev *dev)
{
	pcm_priv_t *pcm_priv = input_get_drvdata(dev);

	usb_kill_urb(pcm_priv->irq_urb);
}

static void hid_int_urb_complete(struct urb *urb)
{
	int ret;
	char *data;
	pcm_priv_t *pcm_priv;
	struct input_dev *inp_dev;

	pcm_priv = urb->context;
	inp_dev = pcm_priv->inp_dev;
	data = pcm_priv->hid_int_buffer;

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
		/* fall through */
	case -ENOENT:
		/* fall through */
	case -ESHUTDOWN:
		return;
	default:
		/* error */
		goto resubmit;
	}

	/* report new input events as required */
	input_report_key(inp_dev, KEY_MUTE, data[0] & VOL_MUTE_BUTTON);
	input_report_key(inp_dev, KEY_VOLUMEUP, data[0] & VOL_UP_BUTTON);
	input_report_key(inp_dev, KEY_VOLUMEDOWN, data[0] & VOL_DN_BUTTON);
	input_sync(inp_dev);

resubmit:
	/* re submit the urb */
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		dev_err(&pcm_priv->usb_dev->dev, "Cannot resubmit intr urb (%d)\n", ret);
}

/***************************** exported functions *****************************/
int pcm_hid_probe(struct usb_interface *iface)
{
	int ret, int_endpoint_size, pipe;
	struct usb_device *device;
	struct usb_host_interface *iface_desc;
	struct input_dev *input_dev;
	pcm_priv_t *pcm_priv;

	device = interface_to_usbdev(iface);
	iface_desc = iface->cur_altsetting;

	/* HID should have exactly one (interrupt) endpoint */
	if (iface_desc->desc.bNumEndpoints != 1)
		return -ENODEV;

	/* input device */
	input_dev = devm_input_allocate_device(&device->dev);
	if (!input_dev)
		goto alloc_err;

	input_dev->name = "PCM HID Input";
	input_dev->dev.parent = &device->dev;
	usb_to_input_id(device, &input_dev->id);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_MUTE, input_dev->keybit);
	set_bit(KEY_VOLUMEUP, input_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, input_dev->keybit);

	input_dev->open = pcm_hid_inp_open;
	input_dev->close = pcm_hid_inp_close;

	/* private data */
	pcm_priv = devm_kzalloc(&iface->dev, sizeof(*pcm_priv), GFP_KERNEL);
	if (!pcm_priv)
		goto alloc_err;

	usb_set_intfdata(iface, pcm_priv);
	input_set_drvdata(input_dev, pcm_priv);

	pcm_priv->usb_dev = device;
	pcm_priv->inp_dev = input_dev;

	/* retrieve the single endpoint of HID interface */
	pcm_priv->hid_int_endpoint = &iface_desc->endpoint[0].desc;
	if (!usb_endpoint_is_int_in(pcm_priv->hid_int_endpoint))
		return -ENODEV;

	/* buffer for HID interrupt msgs */
	pipe = usb_rcvintpipe(device, pcm_priv->hid_int_endpoint->bEndpointAddress);
	int_endpoint_size = usb_maxpacket(device, pipe, usb_pipeout(pipe));

	pcm_priv->hid_int_buffer = devm_kzalloc(&iface->dev, int_endpoint_size, GFP_KERNEL);
	if (!pcm_priv->hid_int_buffer)
		goto alloc_err;

	/**
	 * INFO:
	 * async transfers can be achieved by filling out a usb_ctrlrequest
	 * (request type, completion callback etc), adding it to the URB
	 * via usb_fill_{control|int|bulk}_urb()
	 * and sending the msg via usb_submit_urb()
	 *
	 * sync (blocking) transfers are done via
	 *  usb_{interrupt|control|bulk}_msg()
	 */

	manual_vendor_readout(iface);

	pcm_priv->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!pcm_priv->irq_urb)
		goto alloc_err;

	usb_fill_int_urb(pcm_priv->irq_urb, device,
					 pipe, pcm_priv->hid_int_buffer,
					 int_endpoint_size,
					 hid_int_urb_complete,
					 pcm_priv,
					 pcm_priv->hid_int_endpoint->bInterval);

	/* register the USB device as a new input device */
	ret = input_register_device(input_dev);
	if (ret)
		goto inp_reg_err;

	return 0;

/* error handling */
alloc_err:
	dev_err(&iface->dev, "Cannot allocate memory!\n");
	return -ENOMEM;

inp_reg_err:
	dev_err(&iface->dev, "Cannot register usb input dev!\n");
	usb_free_urb(pcm_priv->irq_urb);
	return ret;
}

void pcm_hid_remove(struct usb_interface *iface)
{
	pcm_priv_t *pcm_priv;

	pcm_priv = usb_get_intfdata(iface);
	if (pcm_priv) {
		usb_free_urb(pcm_priv->irq_urb);
		input_unregister_device(pcm_priv->inp_dev);
	}
}
