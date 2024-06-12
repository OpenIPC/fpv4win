//
// Created by Talus on 2024/6/10.
//

#include "WFBReceiver.h"
#include "QmlNativeAPI.h"
#include "WiFiDriver.h"
#include "logger.h"

#include <iomanip>
#include <sstream>

libusb_context *WFBReceiver::ctx{};
libusb_device_handle *WFBReceiver::dev_handle{};
std::shared_ptr<std::thread> WFBReceiver::usbThread{};

std::vector<std::string> WFBReceiver::GetDongleList() {
  std::vector<std::string> list;

  libusb_context *findctx;
  // Initialize libusb
  libusb_init(&findctx);

  // Get list of USB devices
  libusb_device **devs;
  ssize_t count = libusb_get_device_list(findctx, &devs);
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
        ss << std::setw(4) << std::setfill('0') << std::hex << desc.idVendor
           << ":";
        ss << std::setw(4) << std::setfill('0') << std::hex << desc.idProduct;
        list.push_back(ss.str());
      }
    }
  }

  // Free the list of devices
  libusb_free_device_list(devs, 1);

  // Deinitialize libusb
  libusb_exit(findctx);
  return list;
}
bool WFBReceiver::Start(const std::string &vidPid, uint8_t channel,
                        int channelWidth) {

  if(usbThread){
    return false;
  }
  int rc;

  // get vid pid
  std::istringstream iss(vidPid);
  unsigned int wifiDeviceVid, wifiDevicePid;
  char c;
  iss >> std::hex >> wifiDeviceVid >> c >> wifiDevicePid;

  auto logger = std::make_shared<Logger>(
      [](const std::string &level, const std::string &msg){
        QmlNativeAPI::Instance().PutLog(level,msg);
  });

  rc = libusb_init(&ctx);
  if (rc < 0) {
    return false;
  }
  dev_handle =
      libusb_open_device_with_vid_pid(ctx, wifiDeviceVid, wifiDevicePid);
  if (dev_handle == nullptr) {
    logger->error("Cannot find device {:04x}:{:04x}", wifiDeviceVid,
                  wifiDevicePid);
    libusb_exit(ctx);
    return false;
  }

  /*Check if kenel driver attached*/
  if (libusb_kernel_driver_active(dev_handle, 0)) {
    rc = libusb_detach_kernel_driver(dev_handle, 0); // detach driver
  }
  rc = libusb_claim_interface(dev_handle, 0);

  if (rc < 0) {
    return false;
  }

  usbThread = std::make_shared<std::thread>([=](){
    WiFiDriver wifi_driver{logger};
    try{
      auto rtlDevice = wifi_driver.CreateRtlDevice(dev_handle);
        rtlDevice->Init(
            [](const Packet & p) {
              handle80211Frame(p);
            },
            SelectedChannel{
                .Channel = channel,
                .ChannelOffset = 0,
                .ChannelWidth = static_cast<ChannelWidth_t>(channelWidth),
            });
    }catch (...){
    }
  });
  usbThread->detach();

  return true;
}
void WFBReceiver::handle80211Frame(const Packet &pkt) {

}
WFBReceiver::~WFBReceiver() {
  auto rc = libusb_release_interface(dev_handle, 0);
  if (rc < 0) {
    // error
  }
  libusb_close(dev_handle);
  libusb_exit(ctx);
}
