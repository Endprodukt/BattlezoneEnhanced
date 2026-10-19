/* capture.h - game frames recorded from MAME (tools/mame/bzcapture.lua).
 *
 * File: "BZCAP1\0\0", then chunks: 4-byte tag, uint32 little-endian length,
 * payload. Tags:
 *   ROMS  0x5000 bytes, 3000-7FFF of the recorded machine (once, first)
 *   FRAM  uint32 frame number, uint32 interrupts so far, RAM 0000-03FF,
 *         vector RAM 2000-2FFF, the math box (16 registers and the result,
 *         int16 each): taken when the main line starts a frame, inside
 *         MAIN's LSR SYNC that shifts a 1 out (SYNC still holds it)
 *   RNDM  the random numbers (182A) the main line read during the frame
 *         before, in order; before each FRAM
 *   IRDS  what the main line read during the frame before of the memory
 *         the interrupt keeps and of the switches (bzcap_watch): 3 bytes
 *         per read, address (little-endian) and value, in order. Of its
 *         polls of BUFRDY only the one that got a buffer, and of SYNC
 *         only what is not MAIN's own poll. (The game never looks at
 *         these on purpose after MAIN, but its "CMP (0,X) to waste time"
 *         uses whatever page 0 holds as an address, SYNC and the
 *         interrupt count included, and so reads a different place when
 *         they differ.)
 *   TIME  one double, seconds of emulated time at the FRAM that follows
 *   POKE  uint16 address, uint8 value: the recording's script wrote this
 *         into the machine just before the FRAM that follows
 * Unknown tags are skipped, so the format can grow.
 */
#ifndef BZ_CAPTURE_H
#define BZ_CAPTURE_H

#include <stdint.h>
#include <stdio.h>

#define BZCAP_RAM_SIZE   0x0400
#define BZCAP_VGRAM_SIZE 0x1000
#define BZCAP_FRAM_SIZE  (8 + BZCAP_RAM_SIZE + BZCAP_VGRAM_SIZE + 34)

typedef struct bzcap_frame {
    uint32_t frame;
    uint32_t nmis;
    uint8_t  ram[BZCAP_RAM_SIZE];
    uint8_t  vgram[BZCAP_VGRAM_SIZE];
    int16_t  mb_reg[16];
    int16_t  mb_result;
} bzcap_frame;

typedef struct bzcap_reader {
    FILE   *f;
    uint8_t rom[0x5000];
    int     have_rom;
} bzcap_reader;

int  bzcap_open(bzcap_reader *r, const char *path);   /* 0 or -1 */
void bzcap_close(bzcap_reader *r);

/* The next chunk: its tag and length in *len, its first min(len, cap) bytes
 * in buf. 1, 0 at the end, -1 on a damaged file. */
int  bzcap_next_chunk(bzcap_reader *r, char tag[4], uint8_t *buf, uint32_t cap, uint32_t *len);

/* A FRAM payload (BZCAP_FRAM_SIZE bytes) as a frame. */
void bzcap_parse_frame(const uint8_t *payload, bzcap_frame *t);

/* The addresses whose reads by the main line are recorded (IRDS): set to 1
 * in watch (0x2000 entries). */
void bzcap_watch_set(uint8_t *watch);
int  bzcap_watched(uint16_t addr);

#endif
