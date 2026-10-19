/* The parts that need no ROMs: the 6502 core, the math box, the vector
 * generator's interpreter, and the C routines' 6502 arithmetic against the
 * core's. */
#include "render/hiscore.h"
#include "avg/avg.h"
#include "core/machine.h"
#include "cpu/m6502.h"
#include "game/game.h"
#include "game/m6502.h"
#include "mbox/mathbox.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 0.01)

/* --- the 6502 ---------------------------------------------------------------- */

static uint8_t mem[0x10000];
static uint8_t rd(void *u, uint16_t a) { (void)u; return mem[a]; }
static void wr(void *u, uint16_t a, uint8_t v) { (void)u; mem[a] = v; }

static void run(m6502 *c, const uint8_t *code, int n, int steps)
{
    memcpy(mem + 0x200, code, (size_t)n);
    c->pc = 0x200;
    for (int i = 0; i < steps; i++) m6502_step(c);
}

static void test_cpu(void)
{
    m6502 c;
    m6502_init(&c, rd, wr, NULL);
    mem[0xfffc] = 0x00; mem[0xfffd] = 0x02;
    m6502_reset(&c);
    CHECK(c.pc == 0x200);

    /* LDA #$50, ADC #$50: overflow, negative, no carry */
    static const uint8_t add[] = { 0xa9, 0x50, 0x18, 0x69, 0x50 };
    run(&c, add, sizeof add, 3);
    CHECK(c.a == 0xa0 && (c.p & M6502_V) && (c.p & M6502_N) && !(c.p & M6502_C));

    /* SED, CLC, LDA #$19, ADC #$03 = $22; then ADC #$99 = $21 with carry */
    static const uint8_t bcd[] = { 0xf8, 0x18, 0xa9, 0x19, 0x69, 0x03, 0x69, 0x99, 0xd8 };
    run(&c, bcd, sizeof bcd, 4);
    CHECK(c.a == 0x22 && !(c.p & M6502_C));
    m6502_step(&c);
    CHECK(c.a == 0x21 && (c.p & M6502_C));
    m6502_step(&c);

    /* JSR / RTS, and the stack */
    static const uint8_t jsr[] = { 0xa2, 0x7f, 0x9a, 0x20, 0x08, 0x02, 0xea, 0xea, 0xa9, 0x42, 0x60 };
    run(&c, jsr, sizeof jsr, 5);
    CHECK(c.a == 0x42 && c.pc == 0x206 && c.s == 0x7f);

    /* a branch across a page takes two cycles more, a taken one in the page one more */
    static const uint8_t br[] = { 0x18, 0x90, 0x00 };
    run(&c, br, sizeof br, 1);
    CHECK(m6502_step(&c) == 3);

    /* NMI: pushes PC and P, goes through FFFA */
    mem[0xfffa] = 0x00; mem[0xfffb] = 0x03; mem[0x300] = 0x40;     /* RTI */
    c.pc = 0x1234; c.s = 0x7f;
    m6502_nmi(&c);
    CHECK(m6502_step(&c) == 7 && c.pc == 0x300 && c.s == 0x7c);
    m6502_step(&c);
    CHECK(c.pc == 0x1234 && c.s == 0x7f);
}

/* --- the C routines' arithmetic against the core's ------------------------------ */

static void test_macros(void)
{
    static bz_machine mach;
    bz_machine *m = &mach;
    m6502 c;
    m6502_init(&c, rd, wr, NULL);
    unsigned bad = 0;
    for (int dec = 0; dec < 2; dec++)
        for (unsigned a = 0; a < 256; a++)
            for (unsigned v = 0; v < 256; v += 3)
                for (unsigned carry = 0; carry < 2; carry++) {
                    /* in decimal mode only valid BCD, as the program adds */
                    if (dec && ((a & 15) > 9 || (a >> 4) > 9 || (v & 15) > 9 || (v >> 4) > 9)) continue;
                    static uint8_t code[2];
                    code[0] = 0x69; code[1] = (uint8_t)v;      /* ADC # */
                    c.a = (uint8_t)a;
                    c.p = (uint8_t)(M6502_U | carry | (dec ? M6502_D : 0));
                    run(&c, code, 2, 1);
                    A = (uint8_t)a; C = (uint8_t)carry; m->r.d = (uint8_t)dec;
                    ADC(v);
                    if (A != c.a || C != (c.p & 1)) bad++;
                    if (dec) continue;
                    if (m->r.z != !!(c.p & M6502_Z) || m->r.n != !!(c.p & M6502_N) || m->r.v != !!(c.p & M6502_V)) bad++;
                    code[0] = 0xe9;                             /* SBC # */
                    c.a = (uint8_t)a;
                    c.p = (uint8_t)(M6502_U | carry);
                    run(&c, code, 2, 1);
                    A = (uint8_t)a; C = (uint8_t)carry;
                    SBC(v);
                    if (A != c.a || C != (c.p & 1) || m->r.z != !!(c.p & M6502_Z) || m->r.n != !!(c.p & M6502_N)) bad++;
                    code[0] = 0xc9;                             /* CMP # */
                    c.a = (uint8_t)a;
                    run(&c, code, 2, 1);
                    A = (uint8_t)a;
                    CMP(v);
                    if (C != (c.p & 1) || m->r.z != !!(c.p & M6502_Z) || m->r.n != !!(c.p & M6502_N)) bad++;
                }
    CHECK(bad == 0);

    /* rotates through the carry */
    A = 0x81; C = 0; ROLA(); CHECK(A == 0x02 && C == 1);
    A = 0x01; C = 1; RORA(); CHECK(A == 0x80 && C == 1 && m->r.n);
    BIT(0x40); CHECK(m->r.v && !m->r.n && m->r.z);
}

/* --- the math box ---------------------------------------------------------------- */

static void mb16(bz_mathbox *mb, unsigned lo, int16_t v)
{
    bz_mathbox_go(mb, lo, (uint8_t)v);
    bz_mathbox_go(mb, lo + 1, (uint8_t)((uint16_t)v >> 8));
}

static void test_mathbox(void)
{
    bz_mathbox mb;
    bz_mathbox_reset(&mb);

    /* MBDIAG's first test: A=7F00, B=E=F=0, X=0100, Y=0080 -> X' = 007F */
    mb16(&mb, 0x00, 0x7f00);
    CHECK(bz_mathbox_hi(&mb) == 0x7f && bz_mathbox_lo(&mb) == 0x00);
    mb16(&mb, 0x02, 0);
    mb16(&mb, 0x04, 0);
    mb16(&mb, 0x06, 0);
    mb16(&mb, 0x08, 0x0100);
    bz_mathbox_go(&mb, 0x0a, 0x80);
    bz_mathbox_go(&mb, 0x0b, 0x00);
    CHECK(bz_mathbox_lo(&mb) == 0x7f && bz_mathbox_hi(&mb) == 0x00);
    bz_mathbox_go(&mb, 0x0c, 0x10);                     /* N = 16 */
    bz_mathbox_go(&mb, 0x12, 0);                        /* Y' = (X-E)*B + (Y-F)*A */
    CHECK(bz_mathbox_lo(&mb) == 0x3f && bz_mathbox_hi(&mb) == 0x00);
    bz_mathbox_go(&mb, 0x18, 0);                        /* Y' low */
    CHECK(bz_mathbox_lo(&mb) == 0x00 && bz_mathbox_hi(&mb) == 0x80);
    bz_mathbox_go(&mb, 0x19, 0);                        /* Y' high */
    CHECK(bz_mathbox_lo(&mb) == 0x3f && bz_mathbox_hi(&mb) == 0x00);
    bz_mathbox_go(&mb, 0x17, 0);                        /* X' */
    CHECK(bz_mathbox_lo(&mb) == 0x7f && bz_mathbox_hi(&mb) == 0x00);
    bz_mathbox_go(&mb, 0x13, 0);                        /* Y'/X' */
    CHECK(bz_mathbox_lo(&mb) == 0x00 && bz_mathbox_hi(&mb) == 0x80);

    /* MBDIAG's second: A=5A00, B=A600, E=F=7FFF, X=Y=0100 -> A6B4 */
    mb16(&mb, 0x00, 0x5a00);
    mb16(&mb, 0x02, (int16_t)0xa600);
    mb16(&mb, 0x04, 0x7fff);
    mb16(&mb, 0x06, 0x7fff);
    mb16(&mb, 0x08, 0x0100);
    bz_mathbox_go(&mb, 0x0a, 0x00);
    bz_mathbox_go(&mb, 0x0b, 0x01);
    CHECK(bz_mathbox_lo(&mb) == 0xb4 && bz_mathbox_hi(&mb) == 0xa6);

    /* the distance: the longer side plus 3/8 of the shorter */
    mb16(&mb, 0x00, 100);
    mb16(&mb, 0x02, 200);
    mb16(&mb, 0x04, 500);                               /* dx = 400 */
    bz_mathbox_go(&mb, 0x06, (uint8_t)1000);
    bz_mathbox_go(&mb, 0x1d, (uint8_t)(1000 >> 8));     /* dy = 800 */
    CHECK(mb.result == 800 + 400 / 4 + 400 / 8);
}

/* --- the vector generator ----------------------------------------------------------- */

static uint8_t vmem[0x8000];
static unsigned at;

static void w(unsigned word) { vmem[at] = (uint8_t)word; vmem[at + 1] = (uint8_t)(word >> 8); at += 2; }

/* The macros, as VGMC.MAC writes them. */
static void VCTR(int dx, int dy, int zz) { w((unsigned)dy & 0x1fff); w(((unsigned)zz << 13) | ((unsigned)dx & 0x1fff)); }
static void SVEC(int dx, int dy, int zz) { w(0x4000 | ((unsigned)zz << 5) | (((unsigned)(dx / 2)) & 0x1f) | (((unsigned)(dy * 0x80)) & 0x1f00)); }
static void STAT(int z, int hi, int in) { w(0x6000 | ((unsigned)hi << 9) | ((unsigned)in << 8) | ((unsigned)z << 4)); }
static void BRITE(int z) { w(0x6400 | ((unsigned)z << 4)); }
static void SCAL(int s, int ls) { w(0x7000 | (unsigned)ls | ((unsigned)s << 8)); }
static void CNTR(void) { w(0x8040); }
static void HALT(void) { w(0x2020); }
static void JSRL(unsigned addr) { w((((addr & 0x1fff) / 2)) | 0xa000); }
static void RTSL(void) { w(0xc000); }
static void JMPL(unsigned addr) { w((((addr & 0x1fff) / 2)) | 0xe000); }

static const double FULL = 255.0 / 256.0;   /* linear scale 0 */

static void test_avg(void)
{
    bz_avg_list l = { 0 };

    /* long and short vectors, a move, the brightness */
    memset(vmem, 0, sizeof vmem);
    at = 0x2000;
    CNTR();
    BRITE(9);
    VCTR(256, -128, 7);
    VCTR(0, 512, 0);        /* a move */
    SVEC(-16, 8, 1);        /* brightness 1: as STAT set it */
    HALT();
    CHECK(bz_avg_run(vmem, 0x2000, &l, 0) == BZ_AVG_OK);
    CHECK(l.count == 2);
    if (l.count == 2) {
        CHECK(NEAR(l.lines[0].x0, 0) && NEAR(l.lines[0].y0, 0));
        CHECK(NEAR(l.lines[0].x1, 256 * FULL) && NEAR(l.lines[0].y1, -128 * FULL));
        CHECK(l.lines[0].intensity == 14 && l.lines[0].z == 7);
        CHECK(NEAR(l.lines[1].x0, 256 * FULL) && NEAR(l.lines[1].y0, 384 * FULL));
        CHECK(NEAR(l.lines[1].x1, 240 * FULL) && NEAR(l.lines[1].y1, 392 * FULL));
        CHECK(l.lines[1].intensity == 9);
    }

    /* the binary scale halves; a subroutine in the vector ROM, called from the buffer */
    memset(vmem, 0, sizeof vmem);
    at = 0x3100;
    VCTR(64, 0, 7);
    RTSL();
    at = 0x2000;
    JMPL(0x2802);
    at = 0x2802;
    CNTR();
    SCAL(1, 0);
    JSRL(0x3100);
    JSRL(0x3100);
    HALT();
    CHECK(bz_avg_run(vmem, 0x2000, &l, 0) == BZ_AVG_OK);
    CHECK(l.count == 2);
    if (l.count == 2) {
        CHECK(NEAR(l.lines[1].x1, 64 * FULL));
        CHECK(l.lines[0].sub == 0x3100 && l.lines[0].src == 0x2806 && l.lines[1].src == 0x2808);
    }

    /* the window: lower left corner at (-100,-50), upper right at (100,50) */
    memset(vmem, 0, sizeof vmem);
    at = 0x2000;
    CNTR();
    VCTR(-100, -50, 0);
    STAT(0, 0, 0);
    CNTR();
    VCTR(100, 50, 0);
    STAT(0, 1, 0);
    CNTR();
    BRITE(15);
    VCTR(-400, 0, 0);
    VCTR(800, 0, 7);        /* across the whole window: cut at both sides */
    VCTR(0, 300, 7);        /* outside: gone */
    HALT();
    CHECK(bz_avg_run(vmem, 0x2000, &l, 0) == BZ_AVG_OK);
    CHECK(l.count == 1);
    if (l.count == 1) {
        CHECK(NEAR(l.lines[0].x0, -100 * FULL) && NEAR(l.lines[0].x1, 100 * FULL));
        CHECK(l.clipped == 2);
    }

    /* a list that never halts */
    memset(vmem, 0, sizeof vmem);
    at = 0x2000;
    JMPL(0x2000);
    CHECK(bz_avg_run(vmem, 0x2000, &l, 1000) == BZ_AVG_LIMIT);
    bz_avg_list_free(&l);
}

/* --- the display list routines ------------------------------------------------------ */

static void test_vgut(void)
{
    static bz_machine mach;
    bz_machine *m = &mach;
    bz_avg_list l = { 0 };
    bz_machine_init(m);

    /* A list written by the game's own routines: center, a move, a vector. */
    m->mem[BZ_VGLIST] = 0x00;
    m->mem[BZ_VGLIST + 1] = 0x20;
    bz_vgcntr(m);
    LDA(1);
    bz_vgsca1(m);
    LDA(25); LDX(0xf6); LDY(0);         /* 100, -40, dark */
    bz_vgvtr(m);
    LDA(0xce); LDX(10); LDY(0xe0);      /* -200, 40, bright */
    bz_vgvtr(m);
    bz_vghalt(m);
    CHECK(bz_peek16(m, BZ_VGLIST) == 0x2000 + 2 + 2 + 4 + 4 + 2 + 2);
    CHECK(bz_avg_run(m->mem, BZ_VECRAM, &l, 0) == BZ_AVG_OK);
    CHECK(l.count == 1);
    if (l.count == 1) {
        CHECK(NEAR(l.lines[0].x0, 50 * FULL) && NEAR(l.lines[0].y0, -20 * FULL));
        CHECK(NEAR(l.lines[0].x1, -50 * FULL) && NEAR(l.lines[0].y1, 0));
        CHECK(l.lines[0].intensity == 14);
    }
    bz_avg_list_free(&l);
}

static void test_hiscore(void)
{
    static bz_machine a, b;
    const char *path = "test_hiscore.bin";
    bz_machine_init(&a);
    bz_machine_init(&b);
    for (int i = 0; i < 30; i++) {
        a.mem[BZ_HSCTBL + i] = (uint8_t)(i % 3 == 2 ? 0 : ((i % 10) << 4 | (i % 7)));
        a.mem[BZ_INITLS + i] = (uint8_t)(0x16 + 2 * (i % 26));
    }
    CHECK(bz_hiscore_save(&a, path) == 0);
    CHECK(bz_hiscore_load(&b, path) == 0);
    CHECK(memcmp(a.mem + BZ_HSCTBL, b.mem + BZ_HSCTBL, 30) == 0);
    CHECK(memcmp(a.mem + BZ_INITLS, b.mem + BZ_INITLS, 30) == 0);
    CHECK(bz_hiscore_stamp(&a) == bz_hiscore_stamp(&b) && bz_hiscore_stamp(&a) != 0);
    uint32_t before = bz_hiscore_stamp(&a);
    a.mem[BZ_HSCTBL + 1] ^= 0x01;
    CHECK(bz_hiscore_stamp(&a) != before);
    a.mem[BZ_GOVER] = 0x80;                 /* initials being entered: not final */
    CHECK(bz_hiscore_stamp(&a) == 0);
    /* a file that is no table leaves the machine alone */
    FILE *f = fopen(path, "wb");
    if (f) { fputs("BZHISC1", f); fputc(0, f); for (int i = 0; i < 60; i++) fputc(0xff, f); fclose(f); }
    memset(b.mem + BZ_HSCTBL, 0x11, 30);
    CHECK(bz_hiscore_load(&b, path) == -1);
    CHECK(b.mem[BZ_HSCTBL] == 0x11);
    CHECK(bz_hiscore_load(&b, "no_such_file.bin") == -1);
    remove(path);
}

int main(void)
{
    test_cpu();
    test_macros();
    test_mathbox();
    test_avg();
    test_vgut();
    test_hiscore();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("core: all checks passed\n");
    return 0;
}
