#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "common/crc.h"
#include "error/error.h"

struct __attribute__((packed)) png_fileSignature {
    char signature[8];
};

struct __attribute__((packed)) png_chunk {
    uint32_t length;
    char chunkType[4];
    void *chunkData;
    uint32_t crc;
};

struct __attribute__((packed)) png_IHDR {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compressionMethod;
    uint8_t filterMethod;
    uint8_t interlaceMethod;
};

struct png_IDAT {
    uint8_t cmf;
    uint8_t flg;
    uint8_t cm;
    uint8_t cinfo;
    uint8_t fcheck;
    uint8_t fdict;
    uint8_t flevel;
    uint32_t data_length;
    uint8_t *data;
    uint32_t adler32;
};

// For combining all the IDAT chunks
struct png_IDAT_stream {
    uint8_t *data;
    size_t length;
};

struct png_PLTE {
    uint32_t length;
    uint8_t *data;
};

struct png_zTXt {
    char *keyword;
    uint8_t *compMethod;
    char *compText;
};

struct png_tRNS {
    uint8_t *alpha;
    uint32_t length;
};

struct png_image {
    struct png_IHDR ihdr;
    struct png_PLTE plte;
    struct png_tRNS trns;
    struct png_IDAT_stream idatStream;
    uint8_t *pixels;
    size_t totalPixelSize;
};

int png_readIDAT(void *data, uint32_t length, struct png_IDAT *idat) {
    struct bitStream bs;
    bitstream_init(&bs, (uint8_t *)data, length);

    uint8_t cmf = ((uint8_t *)data)[0];
    uint8_t flg = ((uint8_t *)data)[1];

    if (((cmf << 8) | flg) % 31 != 0) {
        gj_set_error("Invalid zlib header\n");
        return -1;
    }

    uint32_t cm, cinfo;
    uint32_t fcheck, fdict, flevel;
    bitstream_read(&bs, 4, &cm);
    bitstream_read(&bs, 4, &cinfo);
    bitstream_read(&bs, 5, &fcheck);
    bitstream_read(&bs, 1, &fdict);
    bitstream_read(&bs, 2, &flevel);

    uint32_t adler32 =
        ((uint32_t)((uint8_t *)data)[length - 4] << 24) |
        ((uint32_t)((uint8_t *)data)[length - 3] << 16) |
        ((uint32_t)((uint8_t *)data)[length - 2] << 8)  |
        ((uint32_t)((uint8_t *)data)[length - 1]);

    // No need to do -2 (for the CM and CINFO bytes)
    // as we are already skipping them by starting data+2
    int data_length = length - 4; // ADLER32: 4 bytes
    if (data_length < 0) {
        gj_set_error("Invalid zlib data length\n");
        return -1;
    }
    idat->cmf = cmf;
    idat->flg = flg;
    idat->cm = cm;
    idat->cinfo = cinfo;
    idat->fcheck = fcheck;
    idat->fdict = fdict;
    idat->flevel = flevel;
    idat->data_length = data_length;
    idat->data = (uint8_t *)data+2;
    idat->adler32 = adler32;
    return 1;
}

int png_decodeFixedHuffmanSymbol(struct bitStream *ds, uint32_t *symbol) {
    uint32_t peeked;

    // Peek up to 9 bits (max code length in fixed Huffman)
    if (bitstream_peek(ds, 9, &peeked) != 0) {
        return -1; // Error reading bits
    }

    uint32_t rev7 = reverse_bits(peeked, 7);
    if (rev7 <= 23) {                 // symbols 256–279 (7-bit codes)
        *symbol = 256 + rev7;
        bitstream_read(ds, 7, &peeked);
        return 0;
    }

    uint32_t rev8 = reverse_bits(peeked, 8);
    if (rev8 >= 0x30 && rev8 <= 0xBF) {      // symbols 0–143 (8-bit codes)
        *symbol = rev8 - 0x30;
        bitstream_read(ds, 8, &peeked);
        return 0;
    } else if (rev8 >= 0xC0 && rev8 <= 0xC7) { // symbols 280–287 (8-bit codes)
        *symbol = 280 + (rev8 - 0xC0);
        bitstream_read(ds, 8, &peeked);
        return 0;
    }

    uint32_t rev9 = reverse_bits(peeked, 9);
    if (rev9 >= 0x190 && rev9 <= 0x1FF) { // symbols 144–255 (9-bit codes)
        *symbol = 144 + (rev9 - 0x190);
        bitstream_read(ds, 9, &peeked);
        return 0;
    }

    return -1;
}

static const uint16_t dist_base[30] = {
    1,2,3,4,             // 0-3
    5,7,9,13,            // 4-7
    17,25,33,49,         // 8-11
    65,97,129,193,       // 12-15
    257,385,513,769,     // 16-19
    1025,1537,2049,3073, // 20-23
    4097,6145,8193,12289,// 24-27
    16385,24577          // 28-29
};
static const uint8_t dist_extra[30] = {
    0,0,0,0,
    1,1,2,2,
    3,3,4,4,
    5,5,6,6,
    7,7,8,8,
    9,9,10,10,
    11,11,12,12,
    13,13
};
static const uint16_t len_base[29] = {
    3,4,5,6,7,8,9,10,    // 257-264
    11,13,15,17,         // 265-268
    19,23,27,31,         // 269-272
    35,43,51,59,         // 273-276
    67,83,99,115,        // 277-280
    131,163,195,227,     // 281-284
    258                  // 285
};
static const uint8_t len_extra[29] = {
    0,0,0,0,0,0,0,0,    // 257-264
    1,1,1,1,            // 265-268
    2,2,2,2,            // 269-272
    3,3,3,3,            // 273-276
    4,4,4,4,            // 277-280
    5,5,5,5,            // 281-284
    0                   // 285
};
int png_lenFromSym(struct bitStream *ds, uint32_t symbol) {
    if (symbol < 257) return -1;

    int length = 0;
    int index = symbol - 257;
    uint32_t extra_val = 0;
    if (len_extra[index] > 0) {
        bitstream_read(ds, len_extra[index], &extra_val);
    }
    length = len_base[index] + extra_val;
    return length;
}

int png_distFromSym(struct bitStream *ds, uint32_t symbol) {
    if (symbol > 29) return -1;
    uint32_t extra_val = 0;
    if (dist_extra[symbol] > 0) {
        bitstream_read(ds, dist_extra[symbol], &extra_val);
    }
    return dist_base[symbol] + extra_val;
}

uint32_t png_decodeSymbol(
    struct bitStream *ds,
    uint32_t *codes,
    uint8_t *lengths,
    uint32_t num_symbols,
    uint32_t max_len
) {
    uint32_t code = 0;

    for (uint32_t len = 1; len <= max_len; len++) {
        uint32_t bit;
        bitstream_read(ds, 1, &bit);
        code = (code << 1) | bit;

        // Check all symbols with this bit length
        for (uint32_t i = 0; i < num_symbols; i++) {
            if (lengths[i] == len && codes[i] == code) {
                return i;  // Found the symbol!
            }
        }
    }

    return 0xFFFFFFFF; // Error: no match found
}

// Decode one symbol - works for both fixed and dynamic Huffman
uint32_t png_decodeLlSymbol(struct bitStream *ds, int is_dynamic, 
                            uint32_t *ll_codes, uint8_t *ll_lengths, uint32_t hlit) {
    if (is_dynamic) {
        return png_decodeSymbol(ds, ll_codes, ll_lengths, hlit, 15);
    } else {
        uint32_t symbol;
        png_decodeFixedHuffmanSymbol(ds, &symbol);
        return symbol;
    }
}

uint32_t decode_dist_symbol(struct bitStream *ds, int is_dynamic,
                            uint32_t *dist_codes, uint8_t *dist_lengths, uint32_t hdist) {
    if (is_dynamic) {
        return png_decodeSymbol(ds, dist_codes, dist_lengths, hdist, 15);
    } else {
        uint32_t dist_sym;
        bitstream_read(ds, 5, &dist_sym);
        return reverse_bits(dist_sym, 5);
    }
}

int png_huffmanDecode(struct bitStream *ds,
                      uint8_t *output,
                      size_t *output_pos,
                      uint32_t expected,
                      int is_dynamic,
                      uint32_t *ll_codes, uint8_t *ll_lengths, uint32_t hlit,
                      uint32_t *dist_codes, uint8_t *dist_lengths, uint32_t hdist) {
    while (1) {
        uint32_t symbol = png_decodeLlSymbol(ds, is_dynamic, ll_codes, ll_lengths, hlit);

        // End of block
        if (symbol == 256) {
            return 0;
        }

        // Literal byte
        if (symbol < 256) {
            if (*output_pos >= expected) {
                gj_set_error("Output buffer overflow\n");
                return -1;
            }
            output[*output_pos] = (uint8_t)symbol;
            (*output_pos)++;
            continue;
        }

        // Length/distance pair (257-285)
        if (symbol >= 257 && symbol <= 285) {
            int length = png_lenFromSym(ds, symbol);

            uint32_t dist_sym = decode_dist_symbol(ds, is_dynamic, 
                                                   dist_codes, dist_lengths, hdist);
            int distance = png_distFromSym(ds, dist_sym);

            if (*output_pos + length > expected) {
                gj_set_error("Output buffer overflow\n");
                return -1;
            }
            for (int i = 0; i < length; i++) {
                output[*output_pos] = output[*output_pos - distance];
                (*output_pos)++;
            }
            continue;
        }

        gj_set_error("Unexpected symbol %u\n", symbol);
        return -1;
    }

    gj_set_error("Unexpected return from huffmanDecode\n");
    return 0;
}

void png_buildCanonicalHuffman(uint8_t *lengths, uint32_t num_symbols,
                               uint32_t *codes, uint32_t max_bits) {
    uint32_t bl_count[16] = {0};

    // Count codes of each length
    for (uint32_t i = 0; i < num_symbols; i++) {
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
        }
    }

    // Find first code for each length
    uint32_t next_code[16] = {0};
    uint32_t code = 0;
    for (uint32_t bits = 1; bits < max_bits; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    // Assign codes to symbols
    for (uint32_t i = 0; i < num_symbols; i++) {
        uint32_t len = lengths[i];
        if (len > 0) {
            codes[i] = next_code[len];
            next_code[len]++;
        }
    }
}

int png_fixedHuffmanDecode(struct bitStream *ds, uint8_t *output,
                           size_t *output_pos, uint32_t expected) {
    return png_huffmanDecode(ds, output, output_pos, expected,
                             0, NULL, NULL, 0, NULL, NULL, 0);
}

int png_dynamicHuffmanDecode(struct bitStream *ds, uint8_t *output,
                             size_t *output_pos, uint32_t expected) {
    static const uint8_t cl_order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
        11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    // Read headers
    uint32_t hlit, hdist, hclen;
    bitstream_read(ds, 5, &hlit); hlit += 257;
    bitstream_read(ds, 5, &hdist); hdist += 1;
    bitstream_read(ds, 4, &hclen); hclen += 4;

    // Read code-length code lengths
    uint8_t cl_lengths[19] = {0};
    for (uint32_t i = 0; i < hclen; i++) {
        uint32_t v;
        bitstream_read(ds, 3, &v);
        cl_lengths[cl_order[i]] = (uint8_t)v;
    }

    // Build code-length tree
    uint32_t codes[19];
    png_buildCanonicalHuffman(cl_lengths, 19, codes, 8);

    // Decode literal/length and distance code lengths
    uint8_t ll_lengths[288] = {0};
    uint8_t dist_lengths[32] = {0};
    uint32_t total_codes = hlit + hdist;
    uint32_t decoded = 0;
    uint8_t last_value = 0;

    while (decoded < total_codes) {
        uint32_t symbol = png_decodeSymbol(ds, codes, cl_lengths, 19, 7);

        if (symbol < 16) {
            uint8_t *target = (decoded < hlit) ? &ll_lengths[decoded] : &dist_lengths[decoded - hlit];
            *target = symbol;
            last_value = symbol;
            decoded++;
        } else if (symbol == 16) {
            uint32_t repeat;
            bitstream_read(ds, 2, &repeat);
            repeat += 3;
            for (uint32_t i = 0; i < repeat && decoded < total_codes; i++) {
                uint8_t *target = (decoded < hlit) ? &ll_lengths[decoded] : &dist_lengths[decoded - hlit];
                *target = last_value;
                decoded++;
            }
        } else if (symbol == 17) {
            uint32_t repeat;
            bitstream_read(ds, 3, &repeat);
            repeat += 3;
            decoded += repeat;
            last_value = 0;
        } else if (symbol == 18) {
            uint32_t repeat;
            bitstream_read(ds, 7, &repeat);
            repeat += 11;
            decoded += repeat;
            last_value = 0;
        }
    }

    // Build literal/length and distance trees
    uint32_t ll_codes[288];
    uint32_t dist_codes[32];
    png_buildCanonicalHuffman(ll_lengths, hlit, ll_codes, 16);
    png_buildCanonicalHuffman(dist_lengths, hdist, dist_codes, 16);

    // Decode using unified function
    return png_huffmanDecode(ds, output, output_pos, expected,
                             1, ll_codes, ll_lengths, hlit,
                             dist_codes, dist_lengths, hdist);
}

int png_nonCompressed(struct bitStream *ds,
                      uint8_t *output,
                      size_t *output_pos,
                      uint32_t expected) {
    uint32_t len, nlen;
    bitstream_align_byte(ds);
    bitstream_read(ds, 16, &len);
    bitstream_read(ds, 16, &nlen);

    if ((len ^ 0xFFFF) != nlen) {
        gj_set_error("Stored block LEN/NLEN mismatch (LEN=%u NLEN=%u)\n", len, nlen);
        return -1;
    }

    if (*output_pos + len > expected) {
        gj_set_error("Stored block would overflow output buffer\n");
        return -1;
    }

    /* ---- Copy raw bytes ---- */
    for (uint32_t i = 0; i < len; i++) {
        uint32_t val;
        bitstream_read(ds, 8, &val);
        output[*output_pos + i] = (uint8_t)val;
    }

    *output_pos += len;
    return 0;
}

uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

int png_compareAdler32(struct png_IDAT *idat, uint8_t *output, size_t output_pos) {
    uint32_t a = 1;
    uint32_t b = 0;

    for (size_t i = 0; i < output_pos; i++) {
        a = (a + output[i]) % 65521;
        b = (b + a) % 65521;
    }

    uint32_t calculated_adler32 = (b << 16) | a;
    if (calculated_adler32 != idat->adler32) {
        return -1;
    }
    return 1;
}

uint8_t *png_processIDAT(void *data, uint32_t length,
                         struct png_IHDR *ihdr,
                         size_t *out_size) {
    int width  = ihdr->width;
    int height = ihdr->height;
    int channels;

    switch (ihdr->colorType) {
        case 2: // RGB
            if (ihdr->bitDepth != 8) {
                gj_set_error("Only 8-bit RGB supported\n");
                return NULL;
            }
            channels = 3;
            break;

        case 3: // Indexed
            if (ihdr->bitDepth != 8) {
                gj_set_error("Only 8-bit indexed supported\n");
                return NULL;
            }
            channels = 1;
            break;

        case 6: // RGBA
            if (ihdr->bitDepth != 8) {
                gj_set_error("Only 8-bit RGBA supported\n");
                return NULL;
            }
            channels = 4;
            break;

        default:
            gj_set_error("Unsupported color type %u\n", ihdr->colorType);
            return NULL;
    }

    struct png_IDAT idat;
    if (png_readIDAT(data, length, &idat) != 1) {
        return NULL;
    }

    // png_printIDAT(&idat);

    struct bitStream ds;
    bitstream_init(&ds, idat.data, idat.data_length);

    size_t expected = height * (width * channels + 1);
    uint8_t *output = malloc(expected);
    if (!output) {
        gj_set_error("Failed to allocate output buffer\n");
        return NULL;
    }

    size_t output_pos = 0;

    /* ---- ZLIB / DEFLATE BLOCK LOOP ---- */
    uint32_t bfinal, btype;
    while (1) {
        bitstream_read(&ds, 1, &bfinal);
        bitstream_read(&ds, 2, &btype);

        int res;
        switch (btype) {
            case 0:
                res = png_nonCompressed(&ds, output, &output_pos, expected);
                break;
            case 1:
                res = png_fixedHuffmanDecode(&ds, output, &output_pos, expected);
                break;
            case 2:
                res = png_dynamicHuffmanDecode(&ds, output, &output_pos, expected);
                break;
            default:
                gj_set_error("Invalud BTYPE (%u)", btype);
                free(output);
                return NULL;
        }

        if (res != 0) {
            gj_set_error("DEFLATE block decode failed\n");
            free(output);
            return NULL;
        }

        if (bfinal) break;
    }

    if (png_compareAdler32(&idat, output, output_pos) != 1) {
        gj_set_error("Adler32 mismatch\n");
    }

    /* ---- PNG FILTERING ---- */
    int row_bytes = width * channels + 1;
    uint8_t *final_output = malloc(width * height * channels);
    if (!final_output) {
        free(output);
        return NULL;
    }

    int idx = 0;

    for (int row = 0; row < height; row++) {
        int row_start = row * row_bytes;
        uint8_t filter = output[row_start];

        for (int i = 0; i < width * channels; i++) {
            uint8_t raw = output[row_start + 1 + i];
            uint8_t recon;

            uint8_t left = (i >= channels) ? final_output[idx - channels] : 0;
            uint8_t up   = (row > 0)  ? final_output[idx - width * channels] : 0;
            uint8_t up_left =
                (row > 0 && i >= channels) ? final_output[idx - width * channels - channels] : 0;

            switch (filter) {
                case 0: recon = raw; break;
                case 1: recon = raw + left; break;
                case 2: recon = raw + up; break;
                case 3: recon = raw + ((left + up) >> 1); break;
                case 4: recon = raw + paeth_predictor(left, up, up_left); break;
                default:
                    gj_set_error("Unknown filter %u\n", filter);
                    free(output);
                    free(final_output);
                    return NULL;
            }

            final_output[idx++] = recon;
        }
    }

    free(output);

    *out_size = width * height * channels;
    return final_output;
}

void png_interpretzTXt(void *data, uint32_t length) {
    if (data == NULL || length == 0) {
        gj_set_error("Invalid zTXt data\n");
        return;
    }

    unsigned char *bytes = (unsigned char *)data;
    struct png_zTXt ztxt;
    ztxt.keyword = (char *)bytes;

    uint32_t keyword_end = 0;

    while (keyword_end < length && bytes[keyword_end] != 0) {
        keyword_end++;
    }

    if (keyword_end >= length || keyword_end + 1 >= length) {
        gj_set_error("Invalid zTXt chunk format\n");
        return;
    }
    ztxt.compMethod = &bytes[keyword_end + 1];
    ztxt.compText = (char *)&bytes[keyword_end + 2];

    // uint32_t compText_length = length - keyword_end - 2;
}

int png_compareCRC(struct png_chunk *chunk) {
    int crc_inp_len = sizeof(chunk->chunkType) + (int)chunk->length;
    unsigned char *buff = malloc(crc_inp_len);
    if (buff == NULL) {
        gj_set_error("Failed to allocate memory for chunk crc buffer\n");
        return -1;
    }
    memcpy(buff, chunk->chunkType, sizeof(chunk->chunkType));
    memcpy(buff + sizeof(chunk->chunkType), chunk->chunkData, chunk->length);

    unsigned long res = crc(buff, crc_inp_len);
    free(buff);
    if (res == chunk->crc) {
        return 1;
    } else {
        return 0;
    }
}

void png_processChunk(struct png_chunk *chunk, struct png_image *image) {
    if (strncmp(chunk->chunkType, "IHDR", 4) == 0) {
        memcpy(&image->ihdr, chunk->chunkData, sizeof(struct png_IHDR));

        image->ihdr.width  = __builtin_bswap32(image->ihdr.width);
        image->ihdr.height = __builtin_bswap32(image->ihdr.height);

        // png_printIHDR((struct png_IHDR *)chunk->chunkData);
    } else if (strncmp(chunk->chunkType, "PLTE", 4) == 0) {
        image->plte.length = chunk->length;
        image->plte.data = chunk->chunkData;
    } else if (strncmp(chunk->chunkType, "IDAT", 4) == 0) {
        size_t old_len = image->idatStream.length;
        size_t new_len = old_len + chunk->length;

        uint8_t *tmp = realloc(image->idatStream.data, new_len);
        if (!tmp) {
            gj_set_error("Failed to realloc IDAT buffer\n");
            return;
        }

        memcpy(tmp + old_len, chunk->chunkData, chunk->length);
        image->idatStream.data = tmp;
        image->idatStream.length = new_len;
    } else if (strncmp(chunk->chunkType, "zTXt", 4) == 0) {
        png_interpretzTXt(chunk->chunkData, chunk->length);
    } else if (strncmp(chunk->chunkType, "tRNS", 4) == 0) {
        image->trns.alpha = chunk->chunkData;
        image->trns.length = chunk->length;
    } else {
    }
    if (!png_compareCRC(chunk)) {
        gj_set_error("CRC NOT MATCHING\n");
    }
}

int png_readFileSignature(FILE *fptr, struct png_fileSignature *fileSignature) {
    if (fread(fileSignature, sizeof(struct png_fileSignature), 1, fptr) != 1) {
        gj_set_error("Failed to read file signature\n");
        return -1;
    }
    return 1;
}

int png_readChunk(FILE *fptr, struct png_chunk *chunk) {
    if (fread(chunk, (sizeof(chunk->length) + sizeof(chunk->chunkType)), 1,
              fptr) != 1) {
        gj_set_error("Failed to read chunk layout\n");
        return -1;
    }

    chunk->length = __builtin_bswap32(chunk->length);
    chunk->chunkData = (void *)malloc(chunk->length);
    if (chunk->chunkData == NULL) {
        gj_set_error("Failed to allocate memory for chunk data\n");
        return -1;
    }
    if (chunk->length == 0) {
        free(chunk->chunkData);
        chunk->chunkData = NULL;
    } else if (fread(chunk->chunkData, chunk->length, 1, fptr) != 1) {
        gj_set_error("Failed to read chunk data\n");
        free(chunk->chunkData);
        chunk->chunkData = NULL;
        return -1;
    }

    if (fread(&chunk->crc, sizeof(chunk->crc), 1, fptr) != 1) {
        gj_set_error("Failed to read chunk crc\n");
        free(chunk->chunkData);
        return -1;
    }
    chunk->crc = __builtin_bswap32(chunk->crc);

    return 1;
}

int png_readChunks(FILE *fptr, struct png_chunk **chunks, struct png_image *image) {
    int chunkCount = 0;

    while (1) {
        if (chunkCount > 0) {
            size_t cSize = (chunkCount + 1) * sizeof(struct png_chunk);
            struct png_chunk *temp = realloc(*chunks, cSize);
            if (temp == NULL) {
                gj_set_error("Failed to allocte memory for chunks\n");
                break;
            }
            *chunks = temp;
        }
        if (png_readChunk(fptr, &(*chunks)[chunkCount]) != 1) {
            gj_set_error("Error reading chunk or end of file\n");
            break;
        }
        png_processChunk(&(*chunks)[chunkCount], image);

        if (strncmp((*chunks)[chunkCount].chunkType, "IEND", 4) == 0) {
            // printf("\n");
            // printf("End of file reached\n");
            chunkCount++;
            break;
        }
        chunkCount++;
    }

    return chunkCount;
}

unsigned char *png_finalImageConstruction(struct png_image *image, struct image_file *image_file) {
    *image_file->width  = image->ihdr.width;
    *image_file->height = image->ihdr.height;

    int has_alpha = (image->trns.length > 0) || image->ihdr.colorType == 6;
    *image_file->channels = has_alpha ? 4 : 3;

    size_t pixel_count = *image_file->width * *image_file->height;
    unsigned char *pixels = malloc(pixel_count * *image_file->channels);
    if (!pixels) {
        gj_set_error("Out of memory\n");
        return NULL;
    }

    /* truecolor (RGB) */
    if (image->ihdr.colorType == 2) {
        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t r = image->pixels[i * 3 + 0];
            uint8_t g = image->pixels[i * 3 + 1];
            uint8_t b = image->pixels[i * 3 + 2];

            pixels[i * *image_file->channels + 0] = r;
            pixels[i * *image_file->channels + 1] = g;
            pixels[i * *image_file->channels + 2] = b;

            if (has_alpha) {
                pixels[i * *image_file->channels + 3] = 255;
            }
        }
        return pixels;
    }
    /* indexed color (PLTE) */
    if (image->ihdr.colorType == 3 && image->plte.length > 0) {
        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t idx = image->pixels[i];
            uint8_t *pal = &image->plte.data[idx * 3];

            pixels[i * *image_file->channels + 0] = pal[0];
            pixels[i * *image_file->channels + 1] = pal[1];
            pixels[i * *image_file->channels + 2] = pal[2];

            if (has_alpha) {
                if (idx < image->trns.length) {
                    // printf("idx: %u\n", idx);
                    pixels[i * *image_file->channels + 3] = image->trns.alpha[idx];
                } else {
                    pixels[i * *image_file->channels + 3] = 255;
                }
            }
        }
        return pixels;
    }

    /* truecolor (RGBA) */
    if (image->ihdr.colorType == 6) {
        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t r = image->pixels[i * 4 + 0];
            uint8_t g = image->pixels[i * 4 + 1];
            uint8_t b = image->pixels[i * 4 + 2];
            uint8_t a = image->pixels[i * 4 + 3];

            pixels[i * *image_file->channels + 0] = r;
            pixels[i * *image_file->channels + 1] = g;
            pixels[i * *image_file->channels + 2] = b;
            pixels[i * *image_file->channels + 3] = a;
        }
        return pixels;
    }
    return NULL;
}

unsigned char *png_open(struct image_file *image_file) {
    FILE *fptr;

    make_crc_table();

    if ((fptr = fopen(image_file->filename, "rb")) == NULL) {
        gj_set_error("Failed to open file %s\n", image_file->filename);
        return NULL;
    }

    struct png_fileSignature png_fileSignature;
    if (png_readFileSignature(fptr, &png_fileSignature) != 1) {
        return NULL;
    }

    struct png_chunk *chunks = malloc(sizeof(struct png_chunk));
    if (chunks == NULL) {
        gj_set_error("Failed to allocte memory for chunks\n");
        return NULL;
    }

    struct png_image image = {0};
    int chunkCount = png_readChunks(fptr, &chunks, &image);
    if (chunkCount < 0) {
        gj_set_error("Error reading chunks\n");
        fclose(fptr);
        return NULL;
    }
    fclose(fptr);

    if (image.idatStream.data) {
        image.pixels = png_processIDAT(
            image.idatStream.data,
            image.idatStream.length,
            &image.ihdr,
            &image.totalPixelSize
        );
    }

    unsigned char *data = png_finalImageConstruction(&image, image_file);

    for (int i = 0; i < chunkCount; ++i) {
        free(chunks[i].chunkData);
    }
    free(chunks);

    return data;
}
