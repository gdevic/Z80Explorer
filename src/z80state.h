#ifndef Z80STATE_H
#define Z80STATE_H

#include "AppTypes.h"
#include <QString>

// Holds chip state, mainly registers and pins
struct z80state
{
    uint16_t ab;                            // Address bus value
    uint8_t db;                             // Data bus value
    pin_t ab0;                              // Address bus 0 hi-Z sample (if one is hi-Z, all of them are)
    pin_t db0;                              // Data bus bit 0 hi-Z sample (if one is hi-Z, all of them are)
    pin_t mreq, iorq, rd, wr;               // Control pins capable of being hi-Z
    bool busrq, clk, intr, nmi, reset, wait;// Input control signals
    bool busak, halt, m1, rfsh;             // Output control signals

    uint16_t af, bc, de, hl;                // General purpose registers
    uint16_t af2, bc2, de2, hl2;            // Alternative set of general purpose registers
    uint16_t ix, iy, sp, ir, wz, pc;        // Indexing and sytem registers

    bool m[6], t[6];                        // M and T cycles
    uint8_t instr;                          // Instruction register value

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
        s += QString("DB:%1 (driving:%2)").arg(hex(z.db,2)).arg((z.db0 == 2) ? "~~" : hex(z.db,2));
        s += QString("\nclk:%1 int:%2 nmi:%3 halt:%4 mreq:%5 iorq:%6 rd:%7 wr:%8 ").arg
                (pin(z.clk),pin(z.intr),pin(z.nmi),pin(z.halt),pin(z.mreq),pin(z.iorq),pin(z.rd),pin(z.wr));
        s += QString("busak:%1 wait:%2 busrq:%3 reset:%4 m1:%5 rfsh:%6\n").arg
                (pin(z.busak),pin(z.wait),pin(z.busrq),pin(z.reset),pin(z.m1),pin(z.rfsh));
    #define MT(x,c) (x==0 ? "_" : ((x==1) ? c : "?"))
        s += QString("M:%1%2%3%4%5%6 ").arg(MT(z.m[0],"1"),MT(z.m[1],"2"),MT(z.m[2],"3"),MT(z.m[3],"4"),MT(z.m[4],"5"),MT(z.m[5],"6"));
        s += QString("T:%1%2%3%4%5%6\n").arg(MT(z.m[0],"1"),MT(z.m[1],"2"),MT(z.m[2],"3"),MT(z.m[3],"4"),MT(z.m[4],"5"),MT(z.m[5],"6"));
    #undef MT
        s += QString("Instr:%1").arg(hex(z.instr,2));
        return s;
    }

    inline static QString hex(uint n, uint width)
    {
        QString x = QString::number(n,16).toUpper();
        return QString("%1").arg(x,width,QChar('0'));
    }

    inline static QString pin(bool p) // Simple on/off
    {
        return p ? "0" : "1";
    }

    inline static QString pin(pin_t p) // on/off and hi-Z
    {
        return p==0 ? "0" : (p==1 ? "1" : (p==2 ? "~" : "?"));
    }
};

#endif // Z80STATE_H
