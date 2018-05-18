/*
 * zlo2vcd.c -- Zlogan raw => VCD (Verilog Change Dump)
 * by Marek Peca <mp@eltvor.cz>, 2017
 * and Martin Jerabek <martin.jerabek01@gmail.com>, 2018
 *
 * Example usage:
 *   ./zlo2vcd < la.bin > la.vcd
 *   ./zlo2vcd < la.bin | sigrok-cli -I vcd -i /dev/stdin -o la.sr
 */

#include "zlo.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <endian.h>
#include <err.h>
//------------------------------------------------------------------------------

#define MIN(a, b) ((a)<(b) ? (a) : (b))

void outp_vcd_sample(FILE *f, unsigned num_ch, uint64_t u)
{
    static uint64_t last_w;
    static uint64_t t = 0;
    static int virgo = 1;
    uint64_t w = u & ((1ULL<<num_ch)-1);
    uint64_t dt = u>>num_ch;
    t += dt + 1;
    if (virgo) {
        last_w = ~w;
        virgo = 0;
    }
    if (w != last_w) {
        uint64_t k, last_x = last_w, x = w;
        fprintf(f, "#%llu", t);
        for (k = 0; k < num_ch; k++, x >>= 1, last_x >>= 1) {
            if ((x ^ last_x) & 0x1) {
                fputc(' ', f);
                fputc('0' + (x&0x1), f);
                fputc('A' + k, f);
            }
        }
        fputc('\n', f);
    }
    last_w = w;
}
//------------------------------------------------------------------------------

void outp_vcd_header(FILE *f, unsigned num_ch)
{
    unsigned k;
    fprintf(f,
            "$version zlo2vcd $end\n"
            "$comment %u channels $end\n"
            "$timescale 10 ns $end\n"
            "$scope module zlogan $end\n", num_ch);
    for (k = 0; k < num_ch; k++) {
        fprintf(f, "$var wire 1 %c x%02u $end\n",
                'A'+k, k);
    }
    fprintf(f,
            "$upscope $end\n"
            "$enddefinitions $end\n");
}
//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    unsigned num_ch, ws, count = 0;
    unsigned DMA_SZ_W;
    size_t k, len;

    zlo_header_t hdr;
    k = fread(&hdr, 1, 4, stdin);
    if (k != 4)
        err(1, "Error: invalid input\n");
    int hdrlen = hdr.hdr_length == 0 ? 8 : hdr.hdr_length;
    len = MIN(hdrlen, sizeof(zlo_header_t))-4;
    k = fread((char*)&hdr+4, 1, len, stdin);
    if (k != len)
        err(1, "Error: invalid input\n");
    if (hdrlen-len)
        fseek(stdin, hdrlen-len, SEEK_CUR);


    DMA_SZ_W = le32toh(hdr.burst_size_w);
    num_ch = hdr.nsignals;
    ws = hdr.word_size;
    fprintf(stderr, "num_ch=%u\n", num_ch);
    if (num_ch > 64-7) {
        fprintf(stderr, "Error: Too many signals\n");
        return 1;
    }

    outp_vcd_header(stdout, num_ch);
    uint64_t u = 0;
    unsigned sh = 0;
    for (k = 0; ; k++) {
        int ch = getchar();
        if (ch == EOF)
            break;
        if (((k/ws) % DMA_SZ_W) == 0) {
            /* skip timestamp */
            fputc('*', stderr);
            fflush(stderr);
            continue;
        }

        // TODO: process parts to allow more signals
        uint8_t b = (uint8_t)ch;
        u |= (uint64_t)(b & 0x7f) << sh;
        if ((b & 0x80) == 0) {
            outp_vcd_sample(stdout, num_ch, u);
            u = 0;
            sh = 0;
            ++count;
        } else {
            sh += 7;
            if (sh >= 64-7)
                fprintf(stderr, "Warning: too big number\n");
        }
    }
    if (sh)
        fprintf(stderr, "Warning: incomplete input\n");
    fprintf(stderr, "count=%u\n", count);
    return 0;
}
