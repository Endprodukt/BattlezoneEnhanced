/* m6502.h - the 6502's registers and instructions as C, for the routines
 * of src/game.
 *
 * The original passes values between its routines in A, X, Y and the
 * carry, tests flags a routine set long before, and leaves scratch values
 * behind that a later frame reads. So that the C computes the same in
 * every bit (tools/bzcompare holds every byte of RAM against MAME), the
 * routines are written with the registers kept in the machine and one
 * macro per instruction; the original's labels and comments stand next to
 * them. All memory goes through bz_rd8/bz_wr8, the hardware included.
 *
 *   LDA(RD(BZ_TANGLE));  SEC();  SBC(RD(BZ_RGOAL));  if (BPL) goto l3;
 *
 * Addressing: RD(sym + X) is ZX or AX, IY(sym) the address of NY,sym.
 * PHA/PLA become a local; PHP/PLP are FLAGS and RESTORE.
 */
#ifndef BZ_GAME_M6502_H
#define BZ_GAME_M6502_H

#include "core/machine.h"

#define A  (m->r.a)
#define X  (m->r.x)
#define Y  (m->r.y)
#define C  (m->r.c)

#define RD(a)     bz_rd8(m, (uint16_t)(a))
#define WR(a, v)  bz_wr8(m, (uint16_t)(a), (uint8_t)(v))
/* The address (zp),Y points at. */
#define IY(zp)    ((uint16_t)((RD(zp) | (RD((zp) + 1) << 8)) + Y))
/* The address (zp,X) points at (the program uses it to waste time). */
#define IX(zp)    ((uint16_t)(RD((uint8_t)((zp) + X)) | (RD((uint8_t)((zp) + X + 1)) << 8)))

static inline uint8_t bz_nz(bz_machine *m, unsigned v)
{
    m->r.z = (uint8_t)((v & 0xff) == 0);
    m->r.n = (uint8_t)((v >> 7) & 1);
    return (uint8_t)v;
}
#define NZ(v) bz_nz(m, (unsigned)(v))

static inline void bz_adc(bz_machine *m, uint8_t v)
{
    if (m->r.d) {
        /* decimal: the program adds valid BCD only and uses the carry, not N, V or Z */
        unsigned lo = (m->r.a & 0x0fu) + (v & 0x0fu) + m->r.c;
        unsigned hi = (m->r.a & 0xf0u) + (v & 0xf0u);
        uint8_t bin = (uint8_t)(m->r.a + v + m->r.c);
        if (lo > 9) { lo += 6; hi += 0x10; }
        m->r.z = (uint8_t)(bin == 0);
        m->r.n = (uint8_t)((hi >> 7) & 1);
        m->r.v = (uint8_t)((~(m->r.a ^ v) & (m->r.a ^ hi) & 0x80) != 0);
        if (hi > 0x90) hi += 0x60;
        m->r.c = (uint8_t)(hi > 0xff);
        m->r.a = (uint8_t)((lo & 0x0f) | (hi & 0xf0));
    } else {
        unsigned sum = (unsigned)m->r.a + v + m->r.c;
        m->r.v = (uint8_t)((~(m->r.a ^ v) & (m->r.a ^ sum) & 0x80) != 0);
        m->r.c = (uint8_t)(sum > 0xff);
        m->r.a = bz_nz(m, sum);
    }
}

static inline void bz_sbc(bz_machine *m, uint8_t v)
{
    /* (the program never subtracts in decimal mode) */
    unsigned diff = (unsigned)m->r.a - v - (m->r.c ? 0u : 1u);
    m->r.v = (uint8_t)(((m->r.a ^ v) & (m->r.a ^ diff) & 0x80) != 0);
    m->r.c = (uint8_t)!(diff & 0x100);
    m->r.a = bz_nz(m, diff);
}

static inline void bz_cmp(bz_machine *m, uint8_t r, uint8_t v)
{
    m->r.c = (uint8_t)(r >= v);
    bz_nz(m, (unsigned)(r - v));
}

static inline void bz_bit(bz_machine *m, uint8_t v)
{
    m->r.z = (uint8_t)((m->r.a & v) == 0);
    m->r.n = (uint8_t)(v >> 7);
    m->r.v = (uint8_t)((v >> 6) & 1);
}

static inline uint8_t bz_asl(bz_machine *m, uint8_t v) { m->r.c = (uint8_t)(v >> 7); return bz_nz(m, (unsigned)v << 1); }
static inline uint8_t bz_lsr(bz_machine *m, uint8_t v) { m->r.c = (uint8_t)(v & 1); return bz_nz(m, (unsigned)v >> 1); }
static inline uint8_t bz_rol(bz_machine *m, uint8_t v)
{
    unsigned r = ((unsigned)v << 1) | m->r.c;
    m->r.c = (uint8_t)(v >> 7);
    return bz_nz(m, r);
}
static inline uint8_t bz_ror(bz_machine *m, uint8_t v)
{
    unsigned r = ((unsigned)v >> 1) | ((unsigned)m->r.c << 7);
    m->r.c = (uint8_t)(v & 1);
    return bz_nz(m, r);
}

/* loads, stores, transfers */
#define LDA(v)   (A = NZ(v))
#define LDX(v)   (X = NZ(v))
#define LDY(v)   (Y = NZ(v))
#define STA(a)   WR(a, A)
#define STX(a)   WR(a, X)
#define STY(a)   WR(a, Y)
#define TAX()    (X = NZ(A))
#define TAY()    (Y = NZ(A))
#define TXA()    (A = NZ(X))
#define TYA()    (A = NZ(Y))
#define INX()    (X = NZ(X + 1))
#define INY()    (Y = NZ(Y + 1))
#define DEX()    (X = NZ(X - 1))
#define DEY()    (Y = NZ(Y - 1))

/* arithmetic and logic on A */
#define ADC(v)   bz_adc(m, (uint8_t)(v))
#define SBC(v)   bz_sbc(m, (uint8_t)(v))
#define AND(v)   (A = NZ(A & (v)))
#define ORA(v)   (A = NZ(A | (v)))
#define EOR(v)   (A = NZ(A ^ (v)))
#define CMP(v)   bz_cmp(m, A, (uint8_t)(v))
#define CPX(v)   bz_cmp(m, X, (uint8_t)(v))
#define CPY(v)   bz_cmp(m, Y, (uint8_t)(v))
#define BIT(v)   bz_bit(m, (uint8_t)(v))
#define ASLA()   (A = bz_asl(m, A))
#define LSRA()   (A = bz_lsr(m, A))
#define ROLA()   (A = bz_rol(m, A))
#define RORA()   (A = bz_ror(m, A))

/* on memory */
#define ASL(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, bz_asl(m, RD(a_))); } while (0)
#define LSR(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, bz_lsr(m, RD(a_))); } while (0)
#define ROL(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, bz_rol(m, RD(a_))); } while (0)
#define ROR(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, bz_ror(m, RD(a_))); } while (0)
#define INC(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, NZ(RD(a_) + 1)); } while (0)
#define DEC(a)   do { uint16_t a_ = (uint16_t)(a); WR(a_, NZ(RD(a_) - 1)); } while (0)

/* flags */
#define CLC()    (C = 0)
#define SEC()    (C = 1)
#define CLD()    (m->r.d = 0)
#define SED()    (m->r.d = 1)
#define BEQ      (m->r.z)
#define BNE      (!m->r.z)
#define BMI      (m->r.n)
#define BPL      (!m->r.n)
#define BCS      (m->r.c)
#define BCC      (!m->r.c)
#define BVS      (m->r.v)
#define BVC      (!m->r.v)

/* PHP ... PLP: the flags kept in a local and brought back. */
typedef struct bz_flags { uint8_t c, z, n, v, d; } bz_flags;
#define FLAGS(p)   bz_flags p = { m->r.c, m->r.z, m->r.n, m->r.v, m->r.d }
#define RESTORE(p) do { m->r.c = (p).c; m->r.z = (p).z; m->r.n = (p).n; m->r.v = (p).v; m->r.d = (p).d; } while (0)
/* PLA sets N and Z. */
#define PLA(v)   (A = NZ(v))

#endif
