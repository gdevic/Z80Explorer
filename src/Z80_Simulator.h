#ifndef Z80SIMULATOR_H
#define Z80SIMULATOR_H

#include <inttypes.h>

extern class Z80Sim sim;

/*
 * Class Z80Sim has a single instance "sim" which implements a Z80 netlist-level simulator
 */
class Z80Sim
{
public:
    void simLoadNetlist(const char *p_z80netlist);
    int simulate();
    void stop();

private:
    bool is_running;
    int FindTransistor(unsigned int x, unsigned int y);
    int GetRegVal(unsigned int reg[]);

    uint8_t memory[65536];
    uint8_t ports[256];

    int lastadr = 0;
    int lastdata = 0;
    int pomadr = 0;
    bool pom_wr = true;
    bool pom_rd = true;
    bool pom_rst = true;
    bool pom_mreq = true;
    bool pom_iorq = true;
    bool pom_halt = true;

    int outcounter = 0;

    unsigned int reg_a[8], reg_f[8], reg_b[8], reg_c[8], reg_d[8], reg_e[8], reg_d2[8], reg_e2[8], reg_h[8], reg_l[8], reg_h2[8], reg_l2[8];
    unsigned int reg_w[8], reg_z[8], reg_pch[8], reg_pcl[8], reg_sph[8], reg_spl[8];
    unsigned int reg_ixh[8], reg_ixl[8], reg_iyh[8], reg_iyl[8], reg_i[8], reg_r[8];
    unsigned int reg_a2[8], reg_f2[8], reg_b2[8], reg_c2[8];

    unsigned int sig_t1, sig_t2, sig_t3, sig_t4, sig_t5, sig_t6;
    unsigned int sig_m1, sig_m2, sig_m3, sig_m4, sig_m5;
};

#endif // Z80SIMULATOR_H
