//
// Created by Talus on 2024/6/10.
//

#ifndef WFBRECEIVER_H
#define WFBRECEIVER_H
#include "FrameParser.h"
#include "Rtl8812aDevice.h"
#include <QUdpSocket>
#include <libusb.h>
#include <string>
#include <thread>
#include <vector>

class WFBReceiver {
public:
  WFBReceiver();
  ~WFBReceiver();
  static WFBReceiver & Instance() {
      static WFBReceiver wfb_receiver;
      return wfb_receiver;
  }
  std::vector<std::string> GetDongleList();
  bool Start(const std::string &vidPid, uint8_t channel,
                    int channelWidth,const std::string& keyPath);
  bool Stop();
  void handle80211Frame(const Packet &pkt);
  void handleRtp(uint8_t *payload,uint16_t packet_size);
protected:
  libusb_context *ctx;
  libusb_device_handle *dev_handle;
  std::shared_ptr<std::thread> usbThread;
  std::unique_ptr<Rtl8812aDevice> rtlDevice;
  std::string keyPath;
};

#endif // WFBRECEIVER_H
