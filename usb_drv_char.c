#include <linux/usb.h>

/*******************************************************************************
* CHAR DEV DRV
*******************************************************************************/
static int pcm_open(struct inode *inode, struct file *file)
{
	// int minor;
	// struct usb_device *device;
	// struct usb_interface *iface;
	// pcm_priv_t *pcm_priv;

	// minor = iminor(inode);
	// iface = usb_find_interface(&pcm_driver, minor);
	// if (!iface)
	// 	return -ENODEV;
	// device = interface_to_usbdev(iface);
	// pcm_priv = usb_get_intfdata(iface);

	// file->private_data = device;

	return 0;
}

static int pcm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t pcm_write(struct file *file, const char __user *user_buf,
						 size_t count, loff_t *off)
{
	return 0;
}

static ssize_t pcm_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *off)
{
	return 0;
}

/****************************** driver structures *****************************/
static struct file_operations pcm_fops = {
	.owner 		= THIS_MODULE,
	.open 		= pcm_open,
	.release	= pcm_release,
	.read		= pcm_read,
	.write		= pcm_write,
};

static struct usb_class_driver pcm_class_drv = {
	.name		= "pcm2704-HID-Drv",
	.fops		= &pcm_fops,
};

/***************************** exported functions *****************************/
int pcm_char_probe(struct usb_interface *iface)
{
    int ret;

    /* register the USB device as a new char device */
    ret = usb_register_dev(iface, &pcm_class_drv);
    if (ret)
        dev_err(&iface->dev, "Cannot register usb char dev!\n");

    return ret;
}

void pcm_char_remove(struct usb_interface *iface)
{
	usb_deregister_dev(iface, &pcm_class_drv);
}
