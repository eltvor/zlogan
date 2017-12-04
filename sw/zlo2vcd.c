/*
 * zlo2vcd.c -- Zlogan raw => VCD (Verilog Change Dump)
 * by Marek Peca <mp@eltvor.cz>, 2017
 *
 * Example usage (zlogan HDL implemented with 12 channels):
 *   ./zlo2vcd 12 < la.bin > la.vcd
 *   ./zlo2vcd 12 < la.bin | sigrok-cli -I vcd -i /dev/stdin -o la.sr
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

void outp_vcd_sample(FILE *f, unsigned num_ch, uint64_t u) {
  static unsigned last_w;
  static unsigned t = 0;
  static int virgo = 1;
  unsigned w = u & ((1<<num_ch)-1);
  unsigned dt = u>>num_ch;
  t += dt + 1;
  if (virgo) {
    last_w = ~w;
    virgo = 0;
  }
  if (w != last_w) {
    unsigned k, last_x = last_w, x = w;
    fprintf(f, "#%u", t);
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

void outp_vcd_header(FILE *f, unsigned num_ch) {
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

int main(int argc, char *argv[]) {
  unsigned num_ch = 4, count = 0;
  size_t k;

  if (argc > 1) {
    num_ch = atoi(argv[1]);
  }
  fprintf(stderr, "num_ch=%u\n", num_ch);

  outp_vcd_header(stdout, num_ch);
  uint64_t u = 0;
  unsigned sh = 0;
  for (k = 0; ; k++) {
    int ch = getchar();
    if (ch == EOF)
      break;
    if (((k >> 2) & ((1<<20)-1)) == 0) {
      /* skip timestamp */
      fprintf(stderr, "*");
      continue;
    }

    uint8_t b = (uint8_t)ch;
    u |= (b & 0x7f) << sh;
    if ((b & 0x80) == 0) {
      outp_vcd_sample(stdout, num_ch, u);
      u = 0;
      sh = 0;
      ++count;
    } else {
      sh += 7;
    }
  }
  fprintf(stderr, "count=%u\n", count);
  return 0;
}
