//
// Created by Talus on 2024/6/12.
//

#ifndef WFBPROCESSOR_H
#define WFBPROCESSOR_H

#include "WFBDefine.h"
#include <functional>

class BaseAggregator {
public:
    virtual void
    process_packet(const uint8_t *buf, size_t size, uint8_t wlan_idx, const uint8_t *antenna, const int8_t *rssi)
        = 0;
};

class Aggregator : public BaseAggregator {
public:
    using DataCB = std::function<void(uint8_t *payload, uint16_t packet_size)>;
    Aggregator(const std::string &keypair, uint64_t epoch, uint32_t channel_id, const DataCB &cb = nullptr);
    ~Aggregator();
    virtual void
    process_packet(const uint8_t *buf, size_t size, uint8_t wlan_idx, const uint8_t *antenna, const int8_t *rssi);

private:
    void init_fec(int k, int n);
    void deinit_fec(void);
    void send_packet(int ring_idx, int fragment_idx);
    void apply_fec(int ring_idx);
    int get_block_ring_idx(uint64_t block_idx);
    int rx_ring_push(void);
    fec_t *fec_p;
    int fec_k; // RS number of primary fragments in block
    int fec_n; // RS total number of fragments in block
    int sockfd;
    uint32_t seq;
    rx_ring_item_t rx_ring[RX_RING_SIZE];
    int rx_ring_front; // current packet
    int rx_ring_alloc; // number of allocated entries
    uint64_t last_known_block; // id of last known block
    uint64_t epoch; // current epoch
    const uint32_t channel_id; // (link_id << 8) + port_number

    // rx->tx keypair
    uint8_t rx_secretkey[crypto_box_SECRETKEYBYTES];
    uint8_t tx_publickey[crypto_box_PUBLICKEYBYTES];
    uint8_t session_key[crypto_aead_chacha20poly1305_KEYBYTES];

    antenna_stat_t antenna_stat;
    uint32_t count_p_all;
    uint32_t count_p_dec_err;
    uint32_t count_p_dec_ok;
    uint32_t count_p_fec_recovered;
    uint32_t count_p_lost;
    uint32_t count_p_bad;
    uint32_t count_p_override;
    // on data output
    DataCB dcb;
};

#endif // WFBPROCESSOR_H
