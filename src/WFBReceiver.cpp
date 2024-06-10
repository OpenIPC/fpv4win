//
// Created by Talus on 2024/6/10.
//

#include "WFBReceiver.h"

#include <iomanip>
#include <libusb.h>
#include <sstream>

std::vector<std::string> WFBReceiver::GetDongleList() {
    std::vector<std::string> list;

    libusb_context *ctx;
    // Initialize libusb
    libusb_init(&ctx);

    // Get list of USB devices
    libusb_device **devs;
    ssize_t count = libusb_get_device_list(nullptr, &devs);
    if (count < 0) {
        return list;
    }

    // Iterate over devices
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *dev = devs[i];
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev, &desc) == 0) {
            // Check if the device is using libusb driver
            if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE) {
                std::stringstream ss;
                ss<<std::setw(4) << std::setfill('0') << std::hex << desc.idVendor<<":";
                ss<<std::setw(4) << std::setfill('0') << std::hex << desc.idProduct;
                list.push_back(ss.str());
            }
        }
    }

    // Free the list of devices
    libusb_free_device_list(devs, 1);

    // Deinitialize libusb
    libusb_exit(ctx);
    return list;
}
