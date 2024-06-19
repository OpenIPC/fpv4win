//
// Created by liangzhuohua on 2024/6/13.
//

#ifndef FPV_WFB_RTP_H
#define FPV_WFB_RTP_H

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtpHeader {
public:
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(_WIN32) && REG_DWORD == REG_DWORD_BIG_ENDIAN
    // 版本号，固定为2
    uint32_t version : 2;
    // padding
    uint32_t padding : 1;
    // 扩展
    uint32_t ext : 1;
    // csrc
    uint32_t csrc : 4;
    // mark
    uint32_t mark : 1;
    // 负载类型
    uint32_t pt : 7;
#else
    // csrc
    uint32_t csrc : 4;
    // 扩展
    uint32_t ext : 1;
    // padding
    uint32_t padding : 1;
    // 版本号，固定为2
    uint32_t version : 2;
    // 负载类型
    uint32_t pt : 7;
    // mark
    uint32_t mark : 1;
#endif
    // 序列号
    uint32_t seq : 16;
    // 时间戳
    uint32_t stamp;
    // ssrc
    uint32_t ssrc;
    // 负载，如果有csrc和ext，前面为 4 * csrc + (4 + 4 * ext_len)
    uint8_t payload;

public:
#define AV_RB16(x) ((((const uint8_t *)(x))[0] << 8) | ((const uint8_t *)(x))[1])

    size_t getCsrcSize() const {
        // 每个csrc占用4字节
        return csrc << 2;
    }

    uint8_t *getCsrcData() {
        if (!csrc) {
            return nullptr;
        }
        return &payload;
    }

    size_t getExtSize() const {
        // rtp有ext
        if (!ext) {
            return 0;
        }
        auto ext_ptr = &payload + getCsrcSize();
        // uint16_t reserved = AV_RB16(ext_ptr);
        // 每个ext占用4字节
        return AV_RB16(ext_ptr + 2) << 2;
    }

    uint16_t getExtReserved() const {
        // rtp有ext
        if (!ext) {
            return 0;
        }
        auto ext_ptr = &payload + getCsrcSize();
        return AV_RB16(ext_ptr);
    }

    uint8_t *getExtData() {
        if (!ext) {
            return nullptr;
        }
        auto ext_ptr = &payload + getCsrcSize();
        // 多出的4个字节分别为reserved、ext_len
        return ext_ptr + 4;
    }

    size_t getPayloadOffset() const {
        // 有ext时，还需要忽略reserved、ext_len 4个字节
        return getCsrcSize() + (ext ? (4 + getExtSize()) : 0);
    }

    uint8_t *getPayloadData() { return &payload + getPayloadOffset(); }

    size_t getPaddingSize(size_t rtp_size) const {
        if (!padding) {
            return 0;
        }
        auto end = (uint8_t *)this + rtp_size - 1;
        return *end;
    }

    ssize_t getPayloadSize(size_t rtp_size) const {
        auto invalid_size = getPayloadOffset() + getPaddingSize(rtp_size);
        return (ssize_t)rtp_size - invalid_size - 12;
    }

    std::string dumpString(size_t rtp_size) const {
        std::stringstream printer;
        printer << "version:" << (int)version << "\r\n";
        printer << "padding:" << getPaddingSize(rtp_size) << "\r\n";
        printer << "ext:" << getExtSize() << "\r\n";
        printer << "csrc:" << getCsrcSize() << "\r\n";
        printer << "mark:" << (int)mark << "\r\n";
        printer << "pt:" << (int)pt << "\r\n";
        printer << "seq:" << ntohs(seq) << "\r\n";
        printer << "stamp:" << ntohl(stamp) << "\r\n";
        printer << "ssrc:" << ntohl(ssrc) << "\r\n";
        printer << "rtp size:" << rtp_size << "\r\n";
        printer << "payload offset:" << getPayloadOffset() << "\r\n";
        printer << "payload size:" << getPayloadSize(rtp_size) << "\r\n";
        return printer.str();
    }

    ///////////////////////////////////////////////////////////////////////
} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

#endif // FPV_WFB_RTP_H
