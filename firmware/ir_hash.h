/*
 * ir_hash.h - protocol-agnostic IR "fingerprint" -> 6-byte IR-Man style code
 *
 * Pure C, no hardware access, so it can be unit-tested on a PC
 * (see tools/test_ir_hash.c).
 *
 * Input : alternating mark/space durations (mark first), in timer ticks.
 * Output: 6 bytes  [h3 h2 h1 h0 n ID]
 *           h3..h0 : 32-bit FNV-1 hash of the relative-length pattern
 *           n      : number of intervals in the frame
 *           ID     : decoder id (0x01 = this hash decoder; reserved for
 *                    future protocol specific decoders)
 *
 * The hash compares every interval with the one two positions earlier
 * (mark vs mark, space vs space) and classifies it as shorter / equal /
 * longer using a 20 % window. This is the same idea as the "hash decode"
 * of the Arduino IRremote library, and it gives a stable, unique code per
 * key for almost any remote without knowing the protocol. IR-Man software
 * only needs "same key -> same 6 bytes", which is exactly what the
 * original IR-Man guarantees.
 */
#ifndef IR_HASH_H
#define IR_HASH_H

#include <stdint.h>

#define IR_ID_HASH 0x01

static inline uint8_t ir_cmp(uint16_t older, uint16_t newer)
{
    if ((uint32_t)newer * 5u < (uint32_t)older * 4u) return 0; /* shorter */
    if ((uint32_t)older * 5u < (uint32_t)newer * 4u) return 2; /* longer  */
    return 1;                                                  /* equal   */
}

static inline void ir_make_code(const uint16_t *iv, uint8_t n, uint8_t out[6])
{
    uint32_t h = 2166136261UL;
    uint8_t i;

    for (i = 0; (uint8_t)(i + 2) < n; i++)
        h = (h * 16777619UL) ^ ir_cmp(iv[i], iv[i + 2]);

    out[0] = (uint8_t)(h >> 24);
    out[1] = (uint8_t)(h >> 16);
    out[2] = (uint8_t)(h >> 8);
    out[3] = (uint8_t)h;
    out[4] = n;
    out[5] = IR_ID_HASH;
}

#endif
