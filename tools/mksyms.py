#!/usr/bin/env python3
"""mksyms.py - the addresses of the Battlezone program's variables, under
their original names (BZONE.MAC, BZSOUN.MAC), as src/core/bz_symbols.h.

  python -I tools/mksyms.py > src/core/bz_symbols.h

The RAM layout is the source's own (.BLKB by .BLKB) and is checked here
against the globals of the load map (BZONE.MAP). ROM labels the map does not
list were read off the disassembly of the release ROMs (MAME set bzonea).
"""
import sys

NPTS, NOBJ, EXNOBJ, HSCNUM, NOROCK, EMCTRS, MECHS = 26, 28, 8, 10, 5, 3, 3

ZP = [  # page 0, BZONE.MAC .BSECT
 ("VGSIZE", 1, "SCALING SIZE"), ("VGBRIT", 1, "VECTOR BRIGHTNESS"), ("VGLIST", 2, "VECTOR LIST POINTER"),
 ("XCOMP", 4, "X COMPONENT FOR VECTOR DELTA (YCOMP = XCOMP+2)"), ("TEMP1", 2, "SCRATCH"), ("TEMP2", 2, ""),
 ("TEMP3", 2, ""),
 ("CRDT", 1, "$$CRDT: GAME CREDITS"), ("EXPCNT", 1, "EXPLOSION COUNTER"), ("SAVE0", 2, "SCRATCH"),
 ("COLFLG", 1, "COLLISION FLAG"), ("SAVE1", 2, "SCRATCH"), ("SNDWRD", 1, "SOUND WORD"), ("SAVE2", 2, "SCRATCH"),
 ("SAVE3", 1, "WORKING STORAGE FOR ROTAT"), ("X1", 2, ""), ("X2", 2, ""), ("Y1", 2, ""), ("Y2", 2, ""),
 ("CNCT", 1, "$CNCT: COIN COUNT"), ("DDEND", 2, "INPUT DIVIDEND FOR DIVIDE"),
 ("FIRECT", 1, "COUNTER FOR SHELL TIMER"),
 ("DQUE", 2, "DEPTH LVE FOR DIMMING OBJECTS"), ("LANGLE", 1, "LSB OF 9 BIT ANGLE IN D7"),
 ("BUFRDY", 2, "BUFFER READY FOR DOUBLE BUFFER SYNCH"), ("TANGLE", 1, "TANK ANGLE"),
 ("NTHETA", 2, "OBJECT'S ORIENTATION, TANK ANGLE"), ("TPOSX", 4, "TANK'S X POSITION"),
 ("TPOSY", 4, "TANK'S Y POSITION"),
 ("CURNTX", 2, "CURRENT BEAM POSITION X"), ("CURNTY", 2, "CURRENT BEAM POSITION Y"),
 ("SYNC", 1, "NMI. MAIN LOOP SYNCHRONIZATION"),
 ("INTCT", 1, "$INTCT: INTERRUPT COUNTER"), ("TBLPTR", 2, "POINTER FOR TABLE"),
 ("PNTTBL", NPTS * 4, "POINT TABLE"),
 ("TRDCTR", 1, "GARBAGE SAVE"), ("GARBAG", 2, "X & Y OF SHELL POSITIONS"), ("SHELLX", 4, ""), ("SHELLY", 4, ""),
 ("SINCX", 4, "X & Y SHELL INCREMENTS"), ("SINCY", 4, ""), ("HITS", 4, "NUMBER OF HITS (SCORE)"),
 ("RGOAL", 1, "TARGET ANGLE FOR ROBOT"), ("SXPCNT", 1, "SHELL EXPLOSION COUNTER"), ("RANGLE", 1, "RADAR ANGLE"),
 ("SANGLE", 1, "SAUCER ORIENTATION"), ("OTPOSX", 2, "OLD POSITIONS FOR TANKS TO BACK OUT OF COLL."),
 ("OTPOSY", 2, ""),
 ("ACTION", 1, "ROBOTS CURRENT DISPOSITION"), ("STATE", 1, "CALIFORNIA (HERE)"), ("FRAME", 1, "FRAME COUNTER"),
 ("CRACK", 1, "CRACKED WINDSHIELD COUNTER"), ("OBJCOL", 1, "OBJECT COLLISION FLAG"),
 ("EIRNGE", 2, "ENEMY IN RANGE FLAG"),
 ("R2D3FL", 1, "BUZZ BOMB FLAG"), ("LIVES", 1, "LIVES REMAINING"),
 ("GOVER", 1, "GAME OVER & HISCORE TABLE DISPLAY"),
 ("ATRACT", 1, "ATTRACT FLAG"), ("SKILL", 1, "SKILL OF PLAYER"), ("PTURN", 1, "AMT FOR PLAYER TO TURN"),
 ("FTIMER", 1, "TIMER FOR ALTERNATING ATTRACT STATES"), ("TIMOUT", 1, "TANK TIME OUT"),
 ("STIMER", 1, "SAUCER TIMER"),
 ("SCOLFG", 1, "SAUCER COLLISION FLAG"), ("SAPOSX", 2, "SAUCER POSITION"), ("SAPOSY", 2, ""),
 ("SCANGL", 1, "SAUCER ANGLE"),
 ("SAINCX", 2, "SAUCER MOTION INCREMENTS"), ("SAINCY", 2, ""), ("SAUCER", 1, "SAUCER FLAG"),
 ("BOUNCE", 1, "COLLISION BOUNCE VIEW AMT"), ("CCTIM", EMCTRS, "$CCTIM: COIN STUFF"), ("CCPAD", 1, ""),
 ("PSTSL", MECHS, "$PSTSL"), ("CNSTT", MECHS, "$CNSTT"), ("LMTIM", 1, "$LMTIM"), ("CMODE", 1, "$CMODE"),
 ("BCCNT", 1, "$BCCNT"),
 ("VTIMER", 1, "VG TIMER"),
 # BZSOUN.MAC .BSECT follows
 ("SOUNDN", 1, "SOUND # IN PROGRESS"),
 ("POINT", 4, "POINTER OFFSET INTO DATA TABLES (IF 0, CHANNEL NOT ACTIVE)"),
 ("CURRENT", 4, "OUTPUT VALUE"), ("FRAMES", 4, "FRAMES UNTIL NEXT CHANGE"), ("COUNT", 4, "COUNT OF TOTAL CHANGES"),
]
P2 = [  # .=200
 ("PTBLX2", NOBJ * 2, "ROTATED & TRANSLATED OBJECT TABLE"), ("PTBLY2", NOBJ * 2, ""), ("PTBLO2", NOBJ * 2, ""),
 ("EXPOSX", EXNOBJ * 2, "EXPLOSION OBJECT TABLE"), ("EXPOSY", EXNOBJ * 2, ""), ("EXPOSO", 1, ""),
 ("ZVELOC", EXNOBJ * 2 - 1, "Z VELOCITY FOR EXPLOSION PIECES"),
 ("EXPOSZ", 2 * EXNOBJ, "Z POSITION FOR EXPLOSION PIECES"),
 ("TDIST", 1, ""), ("BLIP", 1, "BLIP TIMER"), ("SINT", 1, "SAUCER INTENSITY"),
 ("PL1MOV", 1, "TEMP FOR POKEY SWITCH INPUT"),
 ("NOR2D3", 1, "# OF R2D3'S -1 THIS GAME"), ("TRAP2", 2, "SECURITY TRAP"),
]
P3 = [  # .=300
 ("HSCTBL", 3 * HSCNUM, "HIGH SCORES"), ("INITLS", 3 * HSCNUM, "INITIALS"), ("DSPLAY", 1, "ATTRACT DISPLAY STATE"),
 ("STINTL", 3, "CURRENT INITIAL INPUT"), ("SAVX", 1, "SCRATCH FOR X"), ("LTIMER", 1, "INITIAL ENTRY TIMOUT"),
 ("LETR", 1, ""),
 ("OLSTAT", 1, ""), ("NWSTAT", 1, "SWITCH STATUSES"), ("SUPER", 1, "SUPER BONUS FLAG"),
 ("LOGOX", 2, "LOGO POSITIONS"),
 ("LOGOZ", 2, ""), ("OLDRND", 1, "POKEY RANDOMNESS IMPROVEMENT"),
 ("LOGO", 2, "-1 IF DISPLAYING LOGO IN ATTRACT"),
 ("OBJTIM", NOROCK, "VOLCANO STUFF"), ("XSPD", NOROCK, ""), ("YSPD", NOROCK, ""), ("XPOSL", NOROCK, ""),
 ("XPOSH", NOROCK, ""),
 ("YPOSL", NOROCK, ""), ("YPOSH", NOROCK, ""), ("TRAP1", 1, "SOFTWARE SECURITY"),
 ("YDISP", 1, "SCREEN YDISPLACEMENT - FOR HISCORE DISPLAY USE"),
]
IO = [
 ("HALT", 0x0800, "IN0: D0-D2 coins (low), D3 slam, D4 self test (low), D5 diag step, D6 VG halted, D7 3 kHz"),
 ("OPTION", 0x0A00, "option switches M10: lives, missile level, bonus, language"),
 ("OPTON2", 0x0C00, "option switches P10: coin mode"),
 ("EMCNT", 0x1000, "coin counters"), ("GOADD", 0x1200, "RESTART FOR VECTOR GEN"), ("WTCHDG", 0x1400, ""),
 ("VSTOP", 0x1600, "vector generator reset"), ("MSTAT", 0x1800, "math box status (D7 busy)"),
 ("MOUTLO", 0x1810, "math box result, low"), ("MOUTHI", 0x1818, "math box result, high"),
 ("POKEY", 0x1820, ""), ("CHAN1F", 0x1820, ""), ("CHAN1V", 0x1821, ""), ("CHAN2F", 0x1822, ""),
 ("CHAN2V", 0x1823, ""),
 ("CHAN3F", 0x1824, ""), ("CHAN3V", 0x1825, ""), ("CHAN4F", 0x1826, ""), ("CHAN4V", 0x1827, ""),
 ("ALLPOT", 0x1828, "POT INPUT WORD (the controls); written: AUDCTL"), ("PRAND", 0x182A, "POKEY random number"),
 ("POTGO", 0x182B, "START POT READ"), ("SKCTL", 0x182F, "POKEY SCAN KEYBOARD CONTROL"),
 ("SOUND", 0x1840, "the discrete sounds latch"), ("MBOXIN", 0x1860, "math box, 32 addresses"),
 ("ACOS", 0x1860, "math box A"), ("BSIN", 0x1862, "math box B"), ("TPOSXT", 0x1864, "math box E"),
 ("TPOSYT", 0x1866, "math box F"), ("XMATRX", 0x1868, "math box X"), ("YMATRX", 0x186A, "math box Y low"),
 ("YSTRT1", 0x186B, "Y high, start multiply"), ("DIVCYC", 0x186C, "N"), ("ZLOW", 0x186D, ""),
 ("ZHI", 0x186E, ""),
 ("DIVOUT", 0x186F, "Z high word (2 bytes)"), ("YSTRT2", 0x1871, "Y high, start the whole mess"),
 ("RSTRT1", 0x1872, "start the Y' multiply"), ("ZDIV", 0x1874, "start Z/X' divide"), ("XPL", 0x1875, ""),
 ("XPH", 0x1876, ""),
 ("DXP", 0x1877, "X' to the output"), ("DYPH", 0x1879, "Y' high to the output"), ("DSTNCE", 0x187D, "distance"),
 ("DST2", 0x187E, "distance, second entry"), ("VECRAM", 0x2000, "VECTOR RAM"),
]
ROM = [  # from BZONE.MAP
 ("HORIZN", 0x3000), ("MTNS", 0x3006), ("UNDERL", 0x33AC), ("VGMSGA", 0x33F0), ("EXPIC", 0x347A),
 ("XCROSS", 0x34CC),
 ("XCROS1", 0x3500), ("RDRING", 0x353C), ("TSYMBL", 0x3590), ("CRACKS", 0x36F8), ("HATCH", 0x370A),
 ("ARCTAN", 0x3785),
 ("BONTBL", 0x3886), ("MISLVL", 0x388A), ("OBJPNT", 0x388E), ("EXPTBX", 0x3FAE), ("EXPTBY", 0x3FBA),
 ("IZVEL", 0x3FC6),
 ("PTBLO1", 0x3FCC), ("LOGOBJ", 0x3FF7), ("MAIN", 0x5000), ("BATINT", 0x531F), ("NMI", 0x558F),
 ("MOOLAH", 0x56C3),
 ("BATBL", 0x76D5), ("TLETBL", 0x774E), ("ZONTBL", 0x77CD), ("SNDON", 0x7985), ("MODSND", 0x79B9),
 ("VGRTSL", 0x7A19),
 ("VGHALT", 0x7A1D), ("VGHEXZ", 0x7A29), ("VGHEX", 0x7A2F), ("VGJMPL", 0x7A49), ("VGJSRL", 0x7A5A),
 ("VGSTA1", 0x7A61),
 ("VGSTAT", 0x7A63), ("VGCNTR", 0x7A6A), ("VGADD2", 0x7A6E), ("VGADD", 0x7A76), ("VGSCA1", 0x7A81),
 ("VGSCAL", 0x7A83),
 ("VGDOT", 0x7A8A), ("VGVTR", 0x7A8E), ("VGVTR1", 0x7A90), ("VGVTR2", 0x7AAB), ("VGVCTR", 0x7AAD),
 ("PWRON", 0x7ACC),
 ("DIGT2S", 0x7B9E), ("DIGTYS", 0x7BA0), ("DIGITS", 0x7BA1),
]
ROM2 = [  # local labels, from the disassembly
 ("SINTBL", 0x5E77, "SIN1's table 99$: 65 words, sine of 0..90 degrees"),
 ("PRXTBL", 0x6139, "COLCHK: how near a shell hits each object"),
 ("PROXTB", 0x6995, "OBJOBJ: how near a tank hits each object"),
 ("ENGMSG", 0x6D93, "MSGS: the messages' addresses, English"),
 ("FRNMSG", 0x6F3A, "French"), ("SPNMSG", 0x70F6, "Spanish"), ("GRMMSG", 0x7288, "German"),
 ("OBJTBL", 0x7472, "pointers to the objects' drawing lists"),
 ("PTBLX1", 0x7681, "the playfield: X of each object"), ("PTBLY1", 0x76AB, "Y of each object"),
 ("PNTRS", 0x7864, "BZSOUN: offsets of each sound's four channels"), ("AUDCV", 0x7884, ""),
 ("IDLEV", 0x7885, ""),
 ("SNDTAB", 0x7886, "BZSOUN's SOUND: base of the sequences"),
 ("FDGTBL", 0x7B7F, "BZSTST: the initials of the high score table at power on"),
]


def lay(items, at):
    out = []
    for name, size, comment in items:
        out.append((name, at, comment))
        at += size
    return out, at


zp, eozp = lay(ZP, 0)
p2, _ = lay(P2, 0x200)
p3, _ = lay(P3, 0x300)
sym = {n: a for n, a, _ in zp + p2 + p3}
CHECK = dict(ACTION=0xC4, ATRACT=0xCE, GOVER=0xCD, POINT=0xEF, PNTTBL=0x3D, SAVE0=0x10, SAVE1=0x13, SNDWRD=0x15,
             TEMP1=8, TEMP2=0xA, TEMP3=0xC, TPOSX=0x2D, TPOSY=0x31, VGLIST=2, XCOMP=4, CCTIM=0xE0, PSTSL=0xE4,
             CNSTT=0xE7, LMTIM=0xEA, CMODE=0xEB, BCCNT=0xEC, INTCT=0x3A, CNCT=0x21, CRDT=0x0E, SOUNDN=0xEE,
             HSCTBL=0x300, INITLS=0x31E, DSPLAY=0x33C, LOGO=0x34B, SYNC=0x39, FRAME=0xC6, TIMOUT=0xD2, CRACK=0xC7,
             R2D3FL=0xCB, BUFRDY=0x28)
bad = [k for k, v in CHECK.items() if sym[k] != v]
if bad:
    sys.exit("layout differs from the load map: " + ", ".join("%s=%X" % (k, sym[k]) for k in bad))

print("/* bz_symbols.h - generated by tools/mksyms.py. Do not edit.\n *\n"
      " * The addresses of the Battlezone program (BZONE.MAC, BZSOUN.MAC, BZONE.MAP;\n"
      " * the release of 1-SEP-81, MAME's set bzonea) under their original names.\n */")
print("#ifndef BZ_SYMBOLS_H\n#define BZ_SYMBOLS_H\n")


def emit(title, rows):
    print("/* %s */" % title)
    for r in rows:
        n, a = r[0], r[1]
        c = r[2] if len(r) > 2 else ""
        print(("#define BZ_%-8s 0x%04X" % (n, a)) + ("   /* %s */" % c.strip() if c.strip() else ""))
    print()


emit("page 0", zp)
print("#define BZ_YCOMP    (BZ_XCOMP + 2)\n#define BZ_EOZP     0x%04X\n" % sym["SOUNDN"])
emit("page 2", p2)
emit("page 3", p3)
emit("hardware", IO)
emit("ROM, global (BZONE.MAP)", ROM)
emit("ROM, local labels", ROM2)
print("#define BZ_NPTS 26\n#define BZ_NOBJ 28\n#define BZ_EXNOBJ 8\n#define BZ_HSCNUM 10\n#define BZ_NOROCK 5\n")
print("#endif")
