//
// Created by Talus on 2024/6/12.
//

#include "WFBProcessor.h"

#include <cassert>
#include <format>
#include <stdexcept>
#include <string>
#include <cinttypes>

using namespace std;



Aggregator::Aggregator(const string &keypair, uint64_t epoch, uint32_t channel_id ,const DataCB & cb) : \
    fec_p(NULL), fec_k(-1), fec_n(-1), seq(0), rx_ring_front(0), rx_ring_alloc(0),
    last_known_block((uint64_t)-1), epoch(epoch), channel_id(channel_id),
    count_p_all(0), count_p_dec_err(0), count_p_dec_ok(0), count_p_fec_recovered(0),
    count_p_lost(0), count_p_bad(0), count_p_override(0),dcb(cb)
{
    memset(session_key, '\0', sizeof(session_key));

    FILE *fp;
    if((fp = fopen(keypair.c_str(), "r")) == NULL)
    {
        throw runtime_error(format("Unable to open {}: {}", keypair.c_str(), strerror(errno)));
    }
    if (fread(rx_secretkey, crypto_box_SECRETKEYBYTES, 1, fp) != 1)
    {
        fclose(fp);
        throw runtime_error(format("Unable to read rx secret key: {}", strerror(errno)));
    }
    if (fread(tx_publickey, crypto_box_PUBLICKEYBYTES, 1, fp) != 1)
    {
        fclose(fp);
        throw runtime_error(format("Unable to read tx public key: {}", strerror(errno)));
    }
    fclose(fp);
}


Aggregator::~Aggregator()
{
    if (fec_p != NULL)
    {
        deinit_fec();
    }
}

void Aggregator::init_fec(int k, int n)
{

    fec_k = k;
    fec_n = n;
    fec_p = fec_new(fec_k, fec_n);

    rx_ring_front = 0;
    rx_ring_alloc = 0;
    last_known_block = (uint64_t)-1;
    seq = 0;

    for(int ring_idx = 0; ring_idx < RX_RING_SIZE; ring_idx++)
    {
        rx_ring[ring_idx].block_idx = 0;
        rx_ring[ring_idx].fragment_to_send_idx = 0;
        rx_ring[ring_idx].has_fragments = 0;
        rx_ring[ring_idx].fragments = new uint8_t*[fec_n];
        for(int i=0; i < fec_n; i++)
        {
            rx_ring[ring_idx].fragments[i] = new uint8_t[MAX_FEC_PAYLOAD];
        }
        rx_ring[ring_idx].fragment_map = new uint8_t[fec_n];
        memset(rx_ring[ring_idx].fragment_map, '\0', fec_n * sizeof(uint8_t));
    }
}

void Aggregator::deinit_fec(void)
{

    for(int ring_idx = 0; ring_idx < RX_RING_SIZE; ring_idx++)
    {
        delete rx_ring[ring_idx].fragment_map;
        for(int i=0; i < fec_n; i++)
        {
            delete rx_ring[ring_idx].fragments[i];
        }
        delete rx_ring[ring_idx].fragments;
    }

    fec_free(fec_p);
    fec_p = NULL;
    fec_k = -1;
    fec_n = -1;
}

int Aggregator::rx_ring_push(void)
{
    if(rx_ring_alloc < RX_RING_SIZE)
    {
        int idx = modN(rx_ring_front + rx_ring_alloc, RX_RING_SIZE);
        rx_ring_alloc += 1;
        return idx;
    }

    /*
      Ring overflow. This means that there are more unfinished blocks than ring size
      Possible solutions:
      1. Increase ring size. Do this if you have large variance of packet travel time throught WiFi card or network stack.
         Some cards can do this due to packet reordering inside, diffent chipset and/or firmware or your RX hosts have different CPU power.
      2. Reduce packet injection speed or try to unify RX hardware.
    */

#if 0
    fprintf(stderr, "Override block 0x%" PRIx64 " flush %d fragments\n", rx_ring[rx_ring_front].block_idx, rx_ring[rx_ring_front].has_fragments);
#endif

    count_p_override += 1;

    for(int f_idx=rx_ring[rx_ring_front].fragment_to_send_idx; f_idx < fec_k; f_idx++)
    {
        if(rx_ring[rx_ring_front].fragment_map[f_idx])
        {
            send_packet(rx_ring_front, f_idx);
        }
    }

    // override last item in ring
    int ring_idx = rx_ring_front;
    rx_ring_front = modN(rx_ring_front + 1, RX_RING_SIZE);
    return ring_idx;
}


int Aggregator::get_block_ring_idx(uint64_t block_idx)
{
    // check if block is already in the ring
    for(int i = rx_ring_front, c = rx_ring_alloc; c > 0; i = modN(i + 1, RX_RING_SIZE), c--)
    {
        if (rx_ring[i].block_idx == block_idx) return i;
    }

    // check if block is already known and not in the ring then it is already processed
    if (last_known_block != (uint64_t)-1 && block_idx <= last_known_block)
    {
        return -1;
    }

    int new_blocks = (int)min(last_known_block != (uint64_t)-1 ? block_idx - last_known_block : 1, (uint64_t)RX_RING_SIZE);
    assert (new_blocks > 0);

    last_known_block = block_idx;
    int ring_idx = -1;

    for(int i = 0; i < new_blocks; i++)
    {
        ring_idx = rx_ring_push();
        rx_ring[ring_idx].block_idx = block_idx + i + 1 - new_blocks;
        rx_ring[ring_idx].fragment_to_send_idx = 0;
        rx_ring[ring_idx].has_fragments = 0;
        memset(rx_ring[ring_idx].fragment_map, '\0', fec_n * sizeof(uint8_t));
    }
    return ring_idx;
}





void Aggregator::process_packet(const uint8_t *buf, size_t size, uint8_t wlan_idx, const uint8_t *antenna, const int8_t *rssi)
{
    wsession_data_t new_session_data;
    count_p_all += 1;

    if(size == 0) return;

    if (size > MAX_FORWARDER_PACKET_SIZE)
    {
        fprintf(stderr, "Long packet (fec payload)\n");
        count_p_bad += 1;
        return;
    }

    switch(buf[0])
    {
    case WFB_PACKET_DATA:
        if(size < sizeof(wblock_hdr_t) + sizeof(wpacket_hdr_t))
        {
            fprintf(stderr, "Short packet (fec header)\n");
            count_p_bad += 1;
            return;
        }
        break;

    case WFB_PACKET_KEY:
        if(size != sizeof(wsession_hdr_t) + sizeof(wsession_data_t) + crypto_box_MACBYTES)
        {
            fprintf(stderr, "Invalid session key packet\n");
            count_p_bad += 1;
            return;
        }

        if(crypto_box_open_easy((uint8_t*)&new_session_data,
                                buf + sizeof(wsession_hdr_t),
                                sizeof(wsession_data_t) + crypto_box_MACBYTES,
                                ((wsession_hdr_t*)buf)->session_nonce,
                                tx_publickey, rx_secretkey) != 0)
        {
            fprintf(stderr, "Unable to decrypt session key\n");
            count_p_dec_err += 1;
            return;
        }

        if (be64toh(new_session_data.epoch) < epoch)
        {
            fprintf(stderr, "Session epoch doesn't match: %" PRIu64 " < %" PRIu64 "\n", be64toh(new_session_data.epoch), epoch);
            count_p_dec_err += 1;
            return;
        }

        if (be32toh(new_session_data.channel_id) != channel_id)
        {
            fprintf(stderr, "Session channel_id doesn't match: %d != %d\n", be32toh(new_session_data.channel_id), channel_id);
            count_p_dec_err += 1;
            return;
        }

        if (new_session_data.fec_type != WFB_FEC_VDM_RS)
        {
            fprintf(stderr, "Unsupported FEC codec type: %d\n", new_session_data.fec_type);
            count_p_dec_err += 1;
            return;
        }

        if (new_session_data.n < 1)
        {
            fprintf(stderr, "Invalid FEC N: %d\n", new_session_data.n);
            count_p_dec_err += 1;
            return;
        }

        if (new_session_data.k < 1 || new_session_data.k > new_session_data.n)
        {
            fprintf(stderr, "Invalid FEC K: %d\n", new_session_data.k);
            count_p_dec_err += 1;
            return;
        }

        count_p_dec_ok += 1;

        if (memcmp(session_key, new_session_data.session_key, sizeof(session_key)) != 0)
        {
            epoch = be64toh(new_session_data.epoch);
            memcpy(session_key, new_session_data.session_key, sizeof(session_key));

            if (fec_p != NULL)
            {
                deinit_fec();
            }

            init_fec(new_session_data.k, new_session_data.n);

            fflush(stdout);

        }
        return;

    default:
        fprintf(stderr, "Unknown packet type 0x%x\n", buf[0]);
        count_p_bad += 1;
        return;
    }

    uint8_t decrypted[MAX_FEC_PAYLOAD];
    long long unsigned int decrypted_len;
    wblock_hdr_t *block_hdr = (wblock_hdr_t*)buf;

    if (crypto_aead_chacha20poly1305_decrypt(decrypted, &decrypted_len,
                                             NULL,
                                             buf + sizeof(wblock_hdr_t), size - sizeof(wblock_hdr_t),
                                             buf,
                                             sizeof(wblock_hdr_t),
                                             (uint8_t*)(&(block_hdr->data_nonce)), session_key) != 0)
    {
        fprintf(stderr, "Unable to decrypt packet #0x%" PRIx64 "\n", be64toh(block_hdr->data_nonce));
        count_p_dec_err += 1;
        return;
    }

    count_p_dec_ok += 1;

    assert(decrypted_len <= MAX_FEC_PAYLOAD);

    uint64_t block_idx = be64toh(block_hdr->data_nonce) >> 8;
    uint8_t fragment_idx = (uint8_t)(be64toh(block_hdr->data_nonce) & 0xff);

    // Should never happend due to generating new session key on tx side
    if (block_idx > MAX_BLOCK_IDX)
    {
        fprintf(stderr, "block_idx overflow\n");
        count_p_bad += 1;
        return;
    }

    if (fragment_idx >= fec_n)
    {
        fprintf(stderr, "Invalid fragment_idx: %d\n", fragment_idx);
        count_p_bad += 1;
        return;
    }

    int ring_idx = get_block_ring_idx(block_idx);

    //ignore already processed blocks
    if (ring_idx < 0) return;

    rx_ring_item_t *p = &rx_ring[ring_idx];

    //ignore already processed fragments
    if (p->fragment_map[fragment_idx]) return;

    memset(p->fragments[fragment_idx], '\0', MAX_FEC_PAYLOAD);
    memcpy(p->fragments[fragment_idx], decrypted, decrypted_len);

    p->fragment_map[fragment_idx] = 1;
    p->has_fragments += 1;

    // Check if we use current (oldest) block
    // then we can optimize and don't wait for all K fragments
    // and send packets if there are no gaps in fragments from the beginning of this block
    if(ring_idx == rx_ring_front)
    {
        // check if any packets without gaps
        while(p->fragment_to_send_idx < fec_k && p->fragment_map[p->fragment_to_send_idx])
        {
            send_packet(ring_idx, p->fragment_to_send_idx);
            p->fragment_to_send_idx += 1;
        }

        // remove block if full
        if(p->fragment_to_send_idx == fec_k)
        {
            rx_ring_front = modN(rx_ring_front + 1, RX_RING_SIZE);
            rx_ring_alloc -= 1;
            assert(rx_ring_alloc >= 0);
            return;
        }
    }

    // 1. This is not the oldest block but with sufficient number of fragments (K) to decode
    // 2. This is the oldest block but with gaps and total number of fragments is K
    if(p->fragment_to_send_idx < fec_k && p->has_fragments == fec_k)
    {
        // send all queued packets in all unfinished blocks before and remove them
        int nrm = modN(ring_idx - rx_ring_front, RX_RING_SIZE);

        while(nrm > 0)
        {
            for(int f_idx=rx_ring[rx_ring_front].fragment_to_send_idx; f_idx < fec_k; f_idx++)
            {
                if(rx_ring[rx_ring_front].fragment_map[f_idx])
                {
                    send_packet(rx_ring_front, f_idx);
                }
            }
            rx_ring_front = modN(rx_ring_front + 1, RX_RING_SIZE);
            rx_ring_alloc -= 1;
            nrm -= 1;
        }

        assert(rx_ring_alloc > 0);
        assert(ring_idx == rx_ring_front);

        // Search for missed data fragments and apply FEC only if needed
        for(int f_idx=p->fragment_to_send_idx; f_idx < fec_k; f_idx++)
        {
            if(! p->fragment_map[f_idx])
            {
                //Recover missed fragments using FEC
                apply_fec(ring_idx);

                // Count total number of recovered fragments
                for(; f_idx < fec_k; f_idx++)
                {
                    if(! p->fragment_map[f_idx])
                    {
                        count_p_fec_recovered += 1;
                    }
                }
                break;
            }
        }

        while(p->fragment_to_send_idx < fec_k)
        {
            send_packet(ring_idx, p->fragment_to_send_idx);
            p->fragment_to_send_idx += 1;
        }

        // remove block
        rx_ring_front = modN(rx_ring_front + 1, RX_RING_SIZE);
        rx_ring_alloc -= 1;
        assert(rx_ring_alloc >= 0);
    }
}

void Aggregator::send_packet(int ring_idx, int fragment_idx)
{
    wpacket_hdr_t* packet_hdr = (wpacket_hdr_t*)(rx_ring[ring_idx].fragments[fragment_idx]);
    uint8_t *payload = (rx_ring[ring_idx].fragments[fragment_idx]) + sizeof(wpacket_hdr_t);
    uint8_t flags = packet_hdr->flags;
    uint16_t packet_size = be16toh(packet_hdr->packet_size);
    uint32_t packet_seq = rx_ring[ring_idx].block_idx * fec_k + fragment_idx;

    if (packet_seq > seq + 1 && seq > 0)
    {
        count_p_lost += (packet_seq - seq - 1);
    }

    seq = packet_seq;

    if(packet_size > MAX_PAYLOAD_SIZE)
    {
        fprintf(stderr, "Corrupted packet %u\n", seq);
        count_p_bad += 1;
    }else if(!(flags & WFB_PACKET_FEC_ONLY))
    {
        if(dcb) {
            dcb(payload,packet_size);
        }
    }
}

void Aggregator::apply_fec(int ring_idx)
{
    assert(fec_k >= 1);
    assert(fec_n >= 1);
    assert(fec_k <= fec_n);
    assert(fec_p != nullptr);

    // 动态分配内存
    unsigned *index = new unsigned[fec_k];
    uint8_t **in_blocks = new uint8_t*[fec_k];
    uint8_t **out_blocks = new uint8_t*[fec_n - fec_k];
    int j = fec_k;
    int ob_idx = 0;

    for(int i = 0; i < fec_k; i++)
    {
        if(rx_ring[ring_idx].fragment_map[i])
        {
            in_blocks[i] = rx_ring[ring_idx].fragments[i];
            index[i] = i;
        }
        else
        {
            for(; j < fec_n; j++)
            {
                if(rx_ring[ring_idx].fragment_map[j])
                {
                    in_blocks[i] = rx_ring[ring_idx].fragments[j];
                    out_blocks[ob_idx++] = rx_ring[ring_idx].fragments[i];
                    index[i] = j;
                    j++;
                    break;
                }
            }
        }
    }

    fec_decode(fec_p, (const uint8_t**)in_blocks, out_blocks, index, MAX_FEC_PAYLOAD);

    // 释放动态分配的内存
    delete[] index;
    delete[] in_blocks;
    delete[] out_blocks;
}