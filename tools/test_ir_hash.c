/* Host-side test:  gcc -O2 -I../firmware test_ir_hash.c -o t && ./t
 * Simulates NEC, Sony-like and RC-5-like frames with timing jitter and
 * checks: (1) same key -> same code under jitter, (2) different keys ->
 * different codes.  Ticks are ~15.5 us (same as the firmware). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_hash.h"

#define US(x) ((x) * 64453UL / 1000000UL)      /* us -> ticks */
static int jit(int v, int j){ int r = v + (rand() % (2*j+1)) - j; return r < 1 ? 1 : r; }

static uint8_t nec(uint32_t code, uint16_t *iv, int j)
{
    uint8_t n = 0; int i;
    iv[n++] = jit(US(9000), j); iv[n++] = jit(US(4500), j);
    for (i = 0; i < 32; i++) {
        iv[n++] = jit(US(562), j);
        iv[n++] = jit((code >> i) & 1 ? US(1687) : US(562), j);
    }
    iv[n++] = jit(US(562), j);
    return n;
}
static uint8_t sony(uint16_t code, uint16_t *iv, int j)
{
    uint8_t n = 0; int i;
    iv[n++] = jit(US(2400), j);
    for (i = 0; i < 12; i++) {
        iv[n++] = jit(US(600), j);
        iv[n++] = jit((code >> i) & 1 ? US(1200) : US(600), j);
    }
    return n - 1;   /* frame ends on a mark */
}
static uint8_t rc5(uint16_t code, uint16_t *iv, int j)
{   /* biphase; merge equal half-bits into runs of 889 us */
    uint8_t n = 0; int i, lvl, run, bit, half[28], h = 0;
    for (i = 13; i >= 0; i--) { bit = (code >> i) & 1; half[h++] = bit ? 0 : 1; half[h++] = bit ? 1 : 0; }
    lvl = half[0]; run = 1;
    for (i = 1; i < h; i++) {
        if (half[i] == lvl) run++;
        else { iv[n++] = jit(US(889) * run, j); lvl = half[i]; run = 1; }
    }
    iv[n++] = jit(US(889) * run, j);
    return n;
}

int main(void)
{
    uint16_t iv[128]; uint8_t a[6], b[6], n;
    int bad = 0, trial, J = 3;   /* +-3 ticks ~ +-46 us jitter */
    uint32_t keys_nec[] = {0x00FF08F7, 0x00FF18E7, 0x00FF28D7, 0x20DF10EF};
    int k, m, collisions = 0, nc = 0;
    uint8_t codes[12][6];

    srand(1);
    for (k = 0; k < 4; k++) {
        n = nec(keys_nec[k], iv, 0); ir_make_code(iv, n, a);
        for (trial = 0; trial < 5000; trial++) { n = nec(keys_nec[k], iv, J); ir_make_code(iv, n, b); if (memcmp(a, b, 6)) bad++; }
        memcpy(codes[nc++], a, 6);
    }
    for (k = 0; k < 3; k++) {
        uint16_t c = 0x010 + k * 7;
        n = sony(c, iv, 0); ir_make_code(iv, n, a);
        for (trial = 0; trial < 5000; trial++) { n = sony(c, iv, J); ir_make_code(iv, n, b); if (memcmp(a, b, 6)) bad++; }
        memcpy(codes[nc++], a, 6);
    }
    for (k = 0; k < 3; k++) {
        uint16_t c = 0x1800 + k * 5;
        n = rc5(c, iv, 0); ir_make_code(iv, n, a);
        for (trial = 0; trial < 5000; trial++) { n = rc5(c, iv, J); ir_make_code(iv, n, b); if (memcmp(a, b, 6)) bad++; }
        memcpy(codes[nc++], a, 6);
    }
    for (k = 0; k < nc; k++) for (m = k + 1; m < nc; m++) if (!memcmp(codes[k], codes[m], 6)) collisions++;
    for (k = 0; k < nc; k++) { printf("key %2d: ", k); for (m = 0; m < 6; m++) printf("%02X ", codes[k][m]); printf("\n"); }
    printf("unstable results under jitter: %d / %d\n", bad, 10 * 5000);
    printf("collisions between different keys: %d\n", collisions);
    return bad || collisions;
}
