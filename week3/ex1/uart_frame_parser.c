#include "uart_frame_parser.h"

#include <stddef.h>
#include <string.h>

#include "params.h"

enum {
    SYNC_BYTE = 0xAA,
    SEQ_OFFSET = 1,
    LEN_OFFSET = 5,
    HEADER_BYTES = 7,
    CRC_OFFSET = HEADER_BYTES + W3E1_PAYLOAD_LEN_BYTES,
    FRAME_BYTES = CRC_OFFSET + 4
};

_Static_assert(W3E1_PAYLOAD_LEN_BYTES <= UINT16_MAX, "Payload must fit LEN");

/* STEP 1: Dùng mảng khung tĩnh và bảng CRC32 phản chiếu 16 phần tử trong Flash.
 * Chỉ tính CRC32 trên payload, mỗi byte xử lý hai nhóm 4 bit. */
static uint8_t s_frame[FRAME_BYTES];
static size_t s_used;
static uint32_t s_expected_seq;
static uart_dma_rx_stats_t s_frames;

static const uint32_t s_crc32_table[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static uint32_t payload_crc32(const uint8_t *payload)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0u; i < W3E1_PAYLOAD_LEN_BYTES; ++i) {
        crc ^= payload[i];
        crc = (crc >> 4u) ^ s_crc32_table[crc & 0xFu];
        crc = (crc >> 4u) ^ s_crc32_table[crc & 0xFu];
    }
    return crc ^ UINT32_MAX;
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static uint32_t read_length(const uint8_t *frame)
{
    return (uint32_t)frame[LEN_OFFSET] | ((uint32_t)frame[LEN_OFFSET + 1] << 8u);
}

/* STEP 2: Sau lỗi, tìm lại 0xAA trong phần đuôi vừa nhận.
 * Nếu đã đủ header thì kiểm tra LEN, sau đó giữ phần đuôi sang lô tiếp theo. */
static void resync(void)
{
    for (size_t start = 1u; start < s_used; ++start) {
        const size_t remaining = s_used - start;
        if (s_frame[start] != SYNC_BYTE) {
            continue;
        }
        if (remaining < HEADER_BYTES || read_length(&s_frame[start]) == W3E1_PAYLOAD_LEN_BYTES) {
            memmove(s_frame, &s_frame[start], remaining);
            s_used = remaining;
            return;
        }
    }
    s_used = 0u;
}

/* STEP 3: Tìm byte đồng bộ, kiểm tra header rồi thu payload và CRC32.
 * Byte 0xAA bên trong khung chỉ được xét lại khi khung hiện tại lỗi. */
bool uart_frame_parser_feed(uint8_t byte)
{
    if (s_used == 0u && byte != SYNC_BYTE) {
        return false;
    }
    s_frame[s_used++] = byte;
    if (s_used == HEADER_BYTES && read_length(s_frame) != W3E1_PAYLOAD_LEN_BYTES) {
        ++s_frames.frames_length_error;
        resync();
        return false;
    }
    if (s_used < FRAME_BYTES) {
        return false;
    }
    if (payload_crc32(&s_frame[HEADER_BYTES]) != read_u32_le(&s_frame[CRC_OFFSET])) {
        ++s_frames.frames_crc_error;
        resync();
        return true;
    }

    /* Hiệu uint32_t xử lý SEQ quay vòng. Nửa khoảng trên được xem là SEQ cũ.
     * Riêng SEQ 0 rơi vào nửa trên nghĩa là host chạy lại script mà MCU chưa reset:
     * đếm seq_restarts, đặt lại SEQ chờ và nhận khung như bình thường. */
    const uint32_t seq = read_u32_le(&s_frame[SEQ_OFFSET]);
    uint32_t gap = seq - s_expected_seq;
    if (seq == 0u && gap >= 0x80000000u) {
        ++s_frames.seq_restarts;
        s_expected_seq = 0u;
        gap = 0u;
    }
    if (gap < 0x80000000u) {
        s_frames.frames_lost += gap;
        s_frames.last_seq = seq;
        s_expected_seq = seq + 1u;
        ++s_frames.frames_ok;
    } else {
        ++s_frames.frames_old_seq;
    }
    s_used = 0u;
    return true;
}

void uart_frame_parser_get_stats(uart_dma_rx_stats_t *out)
{
    *out = s_frames;
}
