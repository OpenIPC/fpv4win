//
// Created by Talus on 2024/6/10.
//

#ifndef WFBRECEIVER_H
#define WFBRECEIVER_H
#include "FrameParser.h"
#include "Rtl8812aDevice.h"
#include <libusb.h>
#include <string>
#include <thread>
#include <vector>

class WFBReceiver {
public:
  WFBReceiver() = default;
  ~WFBReceiver();
  static std::vector<std::string> GetDongleList();
  static bool Start(const std::string &vidPid, uint8_t channel,
                    int channelWidth);
  static bool Stop();
  static void handle80211Frame(const Packet &pkt);
protected:
  static libusb_context *ctx;
  static libusb_device_handle *dev_handle;
  static std::shared_ptr<std::thread> usbThread;
  static std::unique_ptr<Rtl8812aDevice> rtlDevice;
};

#endif // WFBRECEIVER_H
