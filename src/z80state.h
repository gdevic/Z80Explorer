#ifndef Z80STATE_H
#define Z80STATE_H

#include "AppTypes.h"
#include <QString>

// Holds portions of the chip state, mainly registers, pins and a few special nets,
// and functions to dump the state in several ways
struct z80state
{
    uint16_t ab;                        // Address bus value
    uint8_t db;                         // Data bus value
    pin_t ab0;                          // Address bus 0 hi-Z sample (if one is hi-Z, all of them are)
    pin_t db0;                          // Data bus bit 0 hi-Z sample (if one is hi-Z, all of them are)
    pin_t mreq, iorq, rd, wr;           // Control pins capable of being hi-Z
    pin_t busrq, clk, intr, nmi, reset, wait;// Input control signals
    pin_t busak, halt, m1, rfsh;        // Output control signals

    uint16_t af, bc, de, hl;            // General purpose registers
    uint16_t af2, bc2, de2, hl2;        // Alternative set of general purpose registers
    uint16_t ix, iy, sp, ir, wz, pc;    // Indexing and sytem registers

    uint8_t instr;                      // Instruction register value
    pin_t nED;                          // ED prefix is active (net 265)
    pin_t nCB;                          // CB prefix is active (net 263)

    /*
     * Returns the chip state structure as a decoded string
     */
    static QString dumpState(z80state z)
    {
        QString s = QString("AF:%1 BC:%2 DE:%3 HL:%4\nAF':%5 BC':%6 DE':%7 HL':%8\n").arg
                (hex(z.af,4),hex(z.bc,4),hex(z.de,4),hex(z.hl,4),
                 hex(z.af2,4),hex(z.bc2,4),hex(z.de2,4),hex(z.hl2,4));
        s += QString("IX:%1 IY:%2 SP:%3 IR:%4 WZ:%5 PC:%6\n").arg
                (hex(z.ix,4),hex(z.iy,4),hex(z.sp,4),hex(z.ir,4),hex(z.wz,4),hex(z.pc,4));
        s += QString("AB:%1 ").arg( (z.ab0 == 2) ? "~~~~" : hex(z.ab,4));
        s += QString("DB:%1 (driving:%2)").arg(hex(z.db,2),(z.db0 == 2) ? "~~" : hex(z.db,2));
        s += QString("\nclk:%1 int:%2 nmi:%3 halt:%4 mreq:%5 iorq:%6 rd:%7 wr:%8 ").arg
                (pin(z.clk),pin(z.intr),pin(z.nmi),pin(z.halt),pin(z.mreq),pin(z.iorq),pin(z.rd),pin(z.wr));
        s += QString("busak:%1 wait:%2 busrq:%3 reset:%4 m1:%5 rfsh:%6\n").arg
                (pin(z.busak),pin(z.wait),pin(z.busrq),pin(z.reset),pin(z.m1),pin(z.rfsh));
        s += QString("Instr:%1 %2\n").arg(hex(z.instr,2),disasm(z.instr, z.nED, z.nCB));
        return s;
    }

    /*
     * Returns Z80 instruction disassembly mnemonics given the ED,CB state modifiers
     */
    static const QString &disasm(uint8_t instr, pin_t nED, pin_t nCB)
    {
        if (!nED) return decodeED[instr];
        if (!nCB) return decodeCB[instr];
        return decode[instr];
    }

private:
    inline static QString hex(uint n, uint width)
    {
        QString x = QString::number(n,16).toUpper();
        return QString("%1").arg(x,width,QChar('0'));
    }

    inline static QString pin(pin_t p) // on/off and hi-Z
    {
        return p==0 ? "0" : (p==1 ? "1" : (p==2 ? "~" : "?"));
    }

    static const QString decode[256];
    static const QString decodeED[256];
    static const QString decodeCB[256];
};

#endif // Z80STATE_H
