//
// Created by Talus on 2024/6/10.
//

#include "WFBReceiver.h"
#include "QmlNativeAPI.h"
#include "RxFrame.h"
#include "WFBProcessor.h"
#include "WiFiDriver.h"
#include "logger.h"

#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>

#include "Rtp.h"

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
                ss << std::setw(4) << std::setfill('0') << std::hex << desc.idVendor << ":";
                ss << std::setw(4) << std::setfill('0') << std::hex << desc.idProduct;
                list.push_back(ss.str());
            }
        }
    }
    std::sort(list.begin(), list.end(), [](std::string &a, std::string &b) {
        static std::vector<std::string> specialStrings = { "0b05:17d2", "0bda:8812", "0bda:881a" };
        auto itA = std::find(specialStrings.begin(), specialStrings.end(), a);
        auto itB = std::find(specialStrings.begin(), specialStrings.end(), b);
        if (itA != specialStrings.end() && itB == specialStrings.end()) {
            return true;
        }
        if (itB != specialStrings.end() && itA == specialStrings.end()) {
            return false;
        }
        return a < b;
    });

    // Free the list of devices
    libusb_free_device_list(devs, 1);

    // Deinitialize libusb
    libusb_exit(findctx);
    return list;
}
bool WFBReceiver::Start(const std::string &vidPid, uint8_t channel, int channelWidth, const std::string &kPath) {

    QmlNativeAPI::Instance().wifiFrameCount_ = 0;
    QmlNativeAPI::Instance().wfbFrameCount_ = 0;
    QmlNativeAPI::Instance().rtpPktCount_ = 0;
    QmlNativeAPI::Instance().UpdateCount();

    keyPath = kPath;
    if (usbThread) {
        return false;
    }
    int rc;

    // get vid pid
    std::istringstream iss(vidPid);
    unsigned int wifiDeviceVid, wifiDevicePid;
    char c;
    iss >> std::hex >> wifiDeviceVid >> c >> wifiDevicePid;

    auto logger = std::make_shared<Logger>(
        [](const std::string &level, const std::string &msg) { QmlNativeAPI::Instance().PutLog(level, msg); });

    rc = libusb_init(&ctx);
    if (rc < 0) {
        return false;
    }
    dev_handle = libusb_open_device_with_vid_pid(ctx, wifiDeviceVid, wifiDevicePid);
    if (dev_handle == nullptr) {
        logger->error("Cannot find device {:04x}:{:04x}", wifiDeviceVid, wifiDevicePid);
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

    usbThread = std::make_shared<std::thread>([=]() {
        WiFiDriver wifi_driver { logger };
        try {
            rtlDevice = wifi_driver.CreateRtlDevice(dev_handle);
            rtlDevice->Init(
                [](const Packet &p) { WFBReceiver::Instance().handle80211Frame(p); },
                SelectedChannel {
                    .Channel = channel,
                    .ChannelOffset = 0,
                    .ChannelWidth = static_cast<ChannelWidth_t>(channelWidth),
                });
        } catch (const std::runtime_error &e) {
            logger->error(e.what());
        } catch (...) {
        }
        auto rc = libusb_release_interface(dev_handle, 0);
        if (rc < 0) {
            // error
        }
        logger->info("==========stoped==========");
        libusb_close(dev_handle);
        libusb_exit(ctx);
        dev_handle = nullptr;
        ctx = nullptr;
        Stop();
        usbThread.reset();
    });
    usbThread->detach();

    return true;
}
void WFBReceiver::handle80211Frame(const Packet &packet) {

    QmlNativeAPI::Instance().wifiFrameCount_++;
    QmlNativeAPI::Instance().UpdateCount();
    RxFrame frame(packet.Data);
    if (!frame.IsValidWfbFrame()) {
        return;
    }
    QmlNativeAPI::Instance().wfbFrameCount_++;
    QmlNativeAPI::Instance().UpdateCount();

    static int8_t rssi[4] = { 1, 1, 1, 1 };
    static uint8_t antenna[4] = { 1, 1, 1, 1 };

    static uint32_t link_id = 7669206; // sha1 hash of link_domain="default"
    static uint8_t video_radio_port = 0;
    static uint64_t epoch = 0;

    static uint32_t video_channel_id_f = (link_id << 8) + video_radio_port;
    static auto video_channel_id_be = htobe32(video_channel_id_f);

    static uint8_t *video_channel_id_be8 = reinterpret_cast<uint8_t *>(&video_channel_id_be);

    static std::mutex agg_mutex;
    static std::unique_ptr<Aggregator> video_aggregator = std::make_unique<Aggregator>(
        keyPath.c_str(), epoch, video_channel_id_f,
        [](uint8_t *payload, uint16_t packet_size) { WFBReceiver::Instance().handleRtp(payload, packet_size); });

    std::lock_guard<std::mutex> lock(agg_mutex);
    if (frame.MatchesChannelID(video_channel_id_be8)) {
        video_aggregator->process_packet(
            packet.Data.data() + sizeof(ieee80211_header), packet.Data.size() - sizeof(ieee80211_header) - 4, 0,
            antenna, rssi);
    }
}

static unsigned long long sendFd = INVALID_SOCKET;
static volatile bool playing = false;

#define GET_H265_NAL_UNIT_TYPE(buffer_ptr) ((buffer_ptr[0] & 0x7E) >> 1)
enum HEVCNALUnitType {
    HEVC_NAL_TRAIL_N    = 0,
    HEVC_NAL_TRAIL_R    = 1,
    HEVC_NAL_TSA_N      = 2,
    HEVC_NAL_TSA_R      = 3,
    HEVC_NAL_STSA_N     = 4,
    HEVC_NAL_STSA_R     = 5,
    HEVC_NAL_RADL_N     = 6,
    HEVC_NAL_RADL_R     = 7,
    HEVC_NAL_RASL_N     = 8,
    HEVC_NAL_RASL_R     = 9,
    HEVC_NAL_BLA_W_LP   = 16,
    HEVC_NAL_BLA_W_RADL = 17,
    HEVC_NAL_BLA_N_LP   = 18,
    HEVC_NAL_IDR_W_RADL = 19,
    HEVC_NAL_IDR_N_LP   = 20,
    HEVC_NAL_CRA_NUT    = 21,
    HEVC_NAL_VPS        = 32,
    HEVC_NAL_SPS        = 33,
    HEVC_NAL_PPS        = 34,
    HEVC_NAL_AUD        = 35,
    HEVC_NAL_EOS_NUT    = 36,
    HEVC_NAL_EOB_NUT    = 37,
    HEVC_NAL_FD_NUT     = 38,
    HEVC_NAL_SEI_PREFIX = 39,
    HEVC_NAL_SEI_SUFFIX = 40,
};
inline bool isH265(const uint8_t * data){
    auto h265NalType = GET_H265_NAL_UNIT_TYPE(data);
    const static std::set<int> tset = {
        HEVC_NAL_TRAIL_N,
        HEVC_NAL_TRAIL_R,
        HEVC_NAL_TSA_N,
        HEVC_NAL_TSA_R,
        HEVC_NAL_STSA_N,
        HEVC_NAL_STSA_R,
        HEVC_NAL_RADL_N,
        HEVC_NAL_RADL_R,
        HEVC_NAL_RASL_N,
        HEVC_NAL_RASL_R,
        HEVC_NAL_BLA_W_LP,
        HEVC_NAL_BLA_W_RADL,
        HEVC_NAL_BLA_N_LP,
        HEVC_NAL_IDR_W_RADL,
        HEVC_NAL_IDR_N_LP,
        HEVC_NAL_CRA_NUT,
        HEVC_NAL_VPS,
        HEVC_NAL_SPS,
        HEVC_NAL_PPS,
        HEVC_NAL_AUD,
        HEVC_NAL_EOS_NUT,
        HEVC_NAL_EOB_NUT,
        HEVC_NAL_FD_NUT,
        HEVC_NAL_SEI_PREFIX,
        HEVC_NAL_SEI_SUFFIX,
    };
    return tset.find(h265NalType) != tset.end();
}

#define GET_H264_NAL_UNIT_TYPE(buffer_ptr) (buffer_ptr[0] & 0x1F)
inline bool isH264(const uint8_t * data){
    // Single NAI Unit Mode = 0. // Single NAI mode (Only nals from 1-23 are allowed)
    // Non Interleaved Mode = 1 // Non-interleaved Mode: 1-23，24 (STAP-A)，28 (FU-A) are allowed
    // Interleaved Mode = 2,  // 25 (STAP-B)，26 (MTAP16)，27 (MTAP24)，28 (EU-A)，and 29 (EU-B) are allowed.
    auto h264NalType = GET_H264_NAL_UNIT_TYPE(data);
    return h264NalType>=1&&h264NalType<=29;
}

void WFBReceiver::handleRtp(uint8_t *payload, uint16_t packet_size) {
    QmlNativeAPI::Instance().rtpPktCount_++;
    QmlNativeAPI::Instance().UpdateCount();
    if (rtlDevice->should_stop) {
        return;
    }
    if (packet_size < 12) {
        return;
    }

    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(QmlNativeAPI::Instance().playerPort);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    auto *header = (RtpHeader *)payload;

    if (!playing) {
        playing = true;
        if(QmlNativeAPI::Instance().playerCodec=="AUTO") {
            // judge H264 or h265
            if (isH264(header->getPayloadData())) {
                QmlNativeAPI::Instance().playerCodec = "H264";
                QmlNativeAPI::Instance().PutLog("debug",
                                                "judge Codec " + QmlNativeAPI::Instance().playerCodec.toStdString());
            } else if (isH265(header->getPayloadData())) {
                QmlNativeAPI::Instance().playerCodec = "H265";
                QmlNativeAPI::Instance().PutLog("debug",
                                                "judge Codec " + QmlNativeAPI::Instance().playerCodec.toStdString());
            }else{
                QmlNativeAPI::Instance().playerCodec = "H264";
                QmlNativeAPI::Instance().PutLog("debug",
                                                "judge Codec failed set codec "
                                                    + QmlNativeAPI::Instance().playerCodec.toStdString());
            }
        }
        QmlNativeAPI::Instance().NotifyRtpStream(header->pt, ntohl(header->ssrc));
    }

    // send video to player
    sendto(
        sendFd, reinterpret_cast<const char *>(payload), packet_size, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
}

bool WFBReceiver::Stop() {
    playing = false;
    if (rtlDevice) {
        rtlDevice->should_stop = true;
    }
    QmlNativeAPI::Instance().NotifyWifiStop();

    return true;
}

WFBReceiver::WFBReceiver() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return;
    }
    sendFd = socket(AF_INET, SOCK_DGRAM, 0);
}

WFBReceiver::~WFBReceiver() {
    closesocket(sendFd);
    sendFd = INVALID_SOCKET;
    WSACleanup();
    Stop();
}
