/*******************************************
 libudev example.

 This example prints out properties of
 each of the hidraw devices. It then
 creates a monitor which will report when
 hidraw devices are connected or removed
 from the system.

 This code is meant to be a teaching
 resource. It can be used for anyone for
 any reason, including embedding into
 a commercial product.
 
 The document describing this file, and
 updated versions can be found at:
    http://www.signal11.us/oss/udev/

 Alan Ott
 Signal 11 Software
 2010-05-22 - Initial Revision
 2010-05-27 - Monitoring initializaion
              moved to before enumeration.
*******************************************/

// https://blog.51cto.com/u_3078781/3288064

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

void test_print_devnode_properties(void);

// hidraw
// block
int main(int argc, char *argv[])
{
    if (argc == 1)
        return 0;
    if (strcmp(argv[1], "test") == 0) {
        test_print_devnode_properties();
        return 0;
    }
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    struct udev_monitor *mon;
    int fd;

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
        printf("Can't create udev\n");
        exit(1);
    }

    /* This section sets up a monitor which will report events when
     devices attached to the system change. Events include "add",
     "remove", "change", "online", and "offline".
     
     This section sets up and starts the monitoring. Events are
     polled for (and delivered) later in the file.
     
     It is important that the monitor be set up before the call to
     udev_enumerate_scan_devices() so that events (and devices) are
     not missed. For example, if enumeration happened first, there
     would be no event generated for a device which was attached after
     enumeration but before monitoring began.
     
     Note that a filter is added so that we only get events for
     "hidraw" devices. */

    // ?????????????????? monitor?????????????????????????????????????????????????????????"kernel"???"udev"???
    // ??????"kernel"????????????????????????"udev"????????????????????????????????????????????????
    // ????????????????????????????????????"udev"???????????????
    mon = udev_monitor_new_from_netlink(udev, "udev");
    // ????????????????????????????????? udev ????????????????????????: "block" ?????????
    udev_monitor_filter_add_match_subsystem_devtype(mon, argv[1], NULL);
    // ??????????????????.
    udev_monitor_enable_receiving(mon);
    /* Get the file descriptor (fd) for the monitor.
     This fd will get passed to select() */
    fd = udev_monitor_get_fd(mon);

    // ?????????????????????????????????????????????????????????
    enumerate = udev_enumerate_new(udev);
    // ???????????????????????????????????????????????????????????????"block"?????????
    udev_enumerate_add_match_subsystem(enumerate, argv[1]);
    // ?????? /sys ????????????????????????????????????????????????
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    /* 
        For each item enumerated, print out its information.
        udev_list_entry_foreach is a macro which expands to
        a loop. The loop will be executed for each member in
        devices, setting dev_list_entry to a list entry
        which contains the device's path in /sys. 
    */
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path;
        // ??????????????????????????? sys ??????.
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        /* 
            usb_device_get_devnode() returns the path to the device node
            itself in /dev. 
        */
        printf("Device Node Path: %s\n", udev_device_get_devnode(dev));
        /*  
            The device pointed to by dev contains information about
            the hidraw device. In order to get information about the
            USB device, get the parent device with the
            subsystem/devtype pair of "usb"/"usb_device". This will
            be several levels up the tree, but the function will find
            it.
        */
        dev = udev_device_get_parent_with_subsystem_devtype(
            dev,
            "usb",
            "usb_device");
        if (!dev) {
            printf("Unable to find parent usb device.\n");
            exit(1);
        }
        /* 
            From here, we can call get_sysattr_value() for each file
            in the device's /sys entry. The strings passed into these
            functions (idProduct, idVendor, serial, etc.) correspond
            directly to the files in the directory which represents
            the USB device. Note that USB strings are Unicode, UCS2
            encoded, but the strings returned from
            udev_device_get_sysattr_value() are UTF-8 encoded. 
        */
        printf(" VID/PID: %s %s\n",
            udev_device_get_sysattr_value(dev, "idVendor"),
            udev_device_get_sysattr_value(dev, "idProduct"));
        printf(" %s\n %s\n",
            udev_device_get_sysattr_value(dev, "manufacturer"),
            udev_device_get_sysattr_value(dev, "product"));
        printf(" serial: %s\n",
            udev_device_get_sysattr_value(dev, "serial"));
        udev_device_unref(dev);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    /* Begin polling for udev events. Events occur when devices
     attached to the system are added, removed, or change state. 
     udev_monitor_receive_device() will return a device
     object representing the device which changed and what type of
     change occured.

     The select() system call is used to ensure that the call to
     udev_monitor_receive_device() will not block.
     
     The monitor was set up earler in this file, and monitoring is
     already underway.
     
     This section will run continuously, calling usleep() at the end
     of each pass. This is to demonstrate how to use a udev_monitor
     in a non-blocking way. */
    while (1) {
        /* Set up the call to select(). In this case, select() will
         only operate on a single file descriptor, the one
         associated with our udev_monitor. Note that the timeval
         object is set to 0, which will cause select() to not
         block. */
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        ret = select(fd + 1, &fds, NULL, NULL, &tv);

        /* Check if our file descriptor has received data. */
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            printf("\nselect() says there should be data\n");

            // ????????????????????????????????????
            dev = udev_monitor_receive_device(mon);
            if (dev) {
                printf("Got Device\n");
                printf(" Node: %s\n", udev_device_get_devnode(dev));
                printf(" Subsystem: %s\n", udev_device_get_subsystem(dev));
                printf(" Devtype: %s\n", udev_device_get_devtype(dev));
                //????????????????????????"add"??????"remove"?????????"change", "online", "offline"???
                printf(" Action: %s\n", udev_device_get_action(dev));
                udev_device_unref(dev);
            } else {
                printf("No Device from receive_device(). An error occured.\n");
            }
        }
        usleep(250 * 1000);
        printf(".");
        fflush(stdout);
    }

    udev_unref(udev);
    return 0;
}

/**
 * ??????/dev/xxx???????????????properties???
 * @devnode: ??????????????????/dev/xxx,???:/dev/ttyUSB0
 */
int print_devnode_properties(const char *devnode)
{
    int ret = 0;
    struct udev *udev;
    struct udev_device *device;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *first_entry;
    struct udev_list_entry *list_entry;
    const char *syspath;

    udev = udev_new();
    enumerate = udev_enumerate_new(udev);

    /*
	 * ?????????????????????????????????,???????????????????????????
	 */
    udev_enumerate_add_match_property(enumerate, "DEVNAME", devnode);

    /*
	 * ???????????????????????????????????????????????????.
	 * ??????: ???????????????????????????,????????????????????????????????????,
	 *       ??????????????????,???????????????
	 */
    udev_enumerate_scan_devices(enumerate);

    list_entry = udev_enumerate_get_list_entry(enumerate);

    if (!list_entry) {
        ret = -1;
        goto ERROR1;
    }

    syspath = udev_list_entry_get_name(list_entry);
    device = udev_device_new_from_syspath(udev, syspath);

    if (!device) {
        ret = -1;
        goto ERROR1;
    }

    first_entry = udev_device_get_properties_list_entry(device);

    if (!first_entry) {
        ret = -1;
        goto ERROR2;
    }

    udev_list_entry_foreach(list_entry, first_entry)
    {
        printf("%s = %s\n", udev_list_entry_get_name(list_entry), udev_list_entry_get_value(list_entry));
    }

ERROR2:
    udev_device_unref(device);
ERROR1:
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return ret;
}

void test_print_devnode_properties(void)
{
    print_devnode_properties("/dev/ttyUSB0");
    print_devnode_properties("/dev/sda1");
    /*
	 * ??????????????????
	 */
    print_devnode_properties("/dev/");
}