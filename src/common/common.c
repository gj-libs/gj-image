#include <stdint.h>
#include <stdio.h>
#include "common/common.h"

// Reverse the lowest n bits of x
uint32_t reverse_bits(uint32_t x, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) {
        r <<= 1;
        r |= (x >> i) & 1;
    }
    return r;
}

void print_binary(uint32_t value, int bits) {
    for (int i = bits - 1; i >= 0; i--) {
        printf("%c", (value & (1 << i)) ? '1' : '0');
    }
    putchar('\n');
}

void bitstream_init(struct bitStream *bs, uint8_t *data, size_t length) {
    bs->data = data;
    bs->bytepos = 0;
    bs->length = length;
}

static uint32_t bit_buffer = 0;
static uint8_t  buffer_left = 0;

int bitstream_read(struct bitStream *bs, int n, uint32_t *out) {
    if (n <= 0 || n > 32) return -1;

    /* Refill buffer until we have enough bits */
    while (buffer_left < n) {
        if (bs->bytepos >= bs->length) {
            return -1; // out of input
        }

        /* Append next byte to buffer (LSB-first) */
        bit_buffer |= ((uint32_t)bs->data[bs->bytepos++]) << buffer_left;
        buffer_left += 8;
    }

    /* Extract lowest n bits */
    *out = bit_buffer & ((1u << n) - 1);

    /* Consume bits */
    bit_buffer >>= n;
    buffer_left -= n;

    return 0;
}

// Write n bits from 'in' to the bitstream (LSB first)
int bitstream_write(struct bitStream *bs, int n, uint32_t in) {
    if (n <= 0 || n > 32) return -1;

    /* Append bits into buffer */
    bit_buffer |= (in & ((1u << n) - 1)) << buffer_left;
    buffer_left += n;

    /* Flush full bytes */
    while (buffer_left >= 8) {
        if (bs->bytepos >= bs->length) return -1;

        bs->data[bs->bytepos++] = bit_buffer & 0xFF;
        bit_buffer >>= 8;
        buffer_left -= 8;
    }

    return 0;
}

int bitstream_flush(struct bitStream *bs) {
    if (buffer_left == 0) return 0;

    if (bs->bytepos >= bs->length) return -1;

    bs->data[bs->bytepos++] = bit_buffer & 0xFF;
    bit_buffer = 0;
    buffer_left = 0;

    return 0;
}

int bitstream_peek(struct bitStream *bs, int n, uint32_t *out) {
    uint32_t saved_buffer = bit_buffer;
    uint8_t  saved_left   = buffer_left;
    size_t   saved_pos    = bs->bytepos;

    int res = bitstream_read(bs, n, out);

    bit_buffer = saved_buffer;
    buffer_left = saved_left;
    bs->bytepos = saved_pos;

    return res;
}

void bitstream_align_byte(struct bitStream *bs) {
    bit_buffer = 0;
    buffer_left = 0;
}

void bitstream_print(struct bitStream *bs) {
    printf("Bitstream (%zu bytes):\n", bs->bytepos);

    for (size_t i = 0; i < bs->bytepos; i++) {
        uint8_t byte = bs->data[i];
        printf("0x%02X  ", byte);
        for (int b = 0; b < 8; b++) {
            printf("%d", (byte >> b) & 1);
        }
        printf("\n");
    }
}

int bitstream_get_size(struct bitStream *bs) {
    return bs->bytepos + (buffer_left ? 1 : 0);
}
