// ChipBrowser.cpp : Defines the entry point for the console application.
// 8085 CPU Simulator
// Version: 1.0
// Author: Pavel Zima
// Date: 1.1.2013
// Adapted to Z80 CPU Simulator on 26.9.2013
// Ported to Linux/g++ by Dave Banks 29.8.2018

#include "Z80_Simulator.h"
#include <string.h>
#include <algorithm>
#include <vector>

// Serialization support
#include "cereal/types/vector.hpp"
#include "cereal/archives/binary.hpp"
#include <fstream>

#if !defined(WINDOWS)
#include <sys/time.h>
#else
#include <Windows.h>
#endif

Z80Sim sim;

using namespace std;

extern void logf(char *fmt, ...);
extern void yield();

unsigned int DIVISOR = 600; // the lower the faster is clock

#define MAXQUANTUM 600.0f // charge gets truncated to this value
#define PULLUPDEFLATOR 1.1f // positive charge gets divided by this

#define GND 1
#define SIG_VCC 2

// definition of pads (pad 4 is skipped)

#define PAD_GND 1   // Pad is optimized out
#define PAD_VCC 2   // Pad is optimized out
#define PAD_CLK 3

#define PAD_A0 5
#define PAD_A1 6
#define PAD_A2 7
#define PAD_A3 8
#define PAD_A4 9
#define PAD_A5 10
#define PAD_A6 11
#define PAD_A7 12
#define PAD_A8 13
#define PAD_A9 14
#define PAD_A10 15
#define PAD_A11 16
#define PAD_A12 17
#define PAD_A13 18
#define PAD_A14 19
#define PAD_A15 20

#define PAD__RESET 21
#define PAD__WAIT 22
#define PAD__INT 23
#define PAD__NMI 24
#define PAD__BUSRQ 25

#define PAD__M1 26
#define PAD__RD 27
#define PAD__WR 28
#define PAD__MREQ 29
#define PAD__IORQ 30
#define PAD__RFSH 31

#define PAD_D0 32
#define PAD_D1 33
#define PAD_D2 34
#define PAD_D3 35
#define PAD_D4 36
#define PAD_D5 37
#define PAD_D6 38
#define PAD_D7 39

#define PAD__HALT 40
#define PAD__BUSAK 41

#define GATE 1
#define DRAIN 2
#define SOURCE 3

// Connection to transistor remembers index of connected transistor and its terminal
// proportion is the proportion of that transisor are to the area of all transistors connected
// to the respective signal - it is here for optimization purposes
class Connection
{
public:
    int terminal {};           // One of: GATE, DRAIN or SOURCE
    int index {};
    float proportion {};

    template <class Archive> void serialize(Archive & ar) { ar(terminal, index, proportion); }
};

#define SIG_GND 1
#define SIG_PWR 2
#define SIG_FLOATING 3

// Signal keeps the vector of Connections (i.e. all transistors connected to the respective signal)
// Homogenize() averages the charge proportionally by transistor area
// ignore means that this signal need not to be homogenized - a try for optimization
// but it works only for Vcc and GND
class Signal
{
public:
    void Homogenize();

    vector<Connection> connections;
    float signalarea {};
    bool ignore {};

    template <class Archive> void serialize(Archive & ar) { ar(connections, signalarea, ignore); }
};

vector<Signal> signals;

#define PAD_INPUT 1
#define PAD_OUTPUT 2
#define PAD_BIDIRECTIONAL 3

// PADs - used for communication with the CPU see its use in simulation below
class Pad
{
public:
    void SetInputSignal(int signal);
    float ReadOutput();
    int ReadOutputStatus();
    int ReadInputStatus();

    int type {};                        // Signal direction; one of: PAD_INPUT, PAD_OUTPUT or PAD_BIDIRECTIONAL
    int x {}, y {};                     // Center coordinates of this pad within the image map
    int origsignal {};                  // Identifies the pad, signal; one of the defines PAD_*
    vector<Connection> connections;     // List of transistors this pad is connected to

    template <class Archive> void serialize(Archive & ar) { ar(type, x, y, origsignal, connections); }
};

vector<Pad> pads;

// transistor - keeps connections to other transistors
// Simulate() - moves charge between source and drain
class Transistor
{
public:
    bool IsOn()       { return m_gatecharge > 0.0f; }
    int IsOnAnalog()  { return int(50.0f * m_gatecharge / m_area) + 50; }
    void Simulate();
    void Normalize();
    int Valuate();

    int x {}, y {};                     // Top-left coordinates of this transistor within the image map
    int gate {}, source {}, drain {};
    int sourcelen {}, drainlen {}, otherlen {};
    float m_area {};                    // Determines the maximum amount of charge that this transistor can have (-area/+area)
    bool depletion {};
    float resist {};
    float m_gatecharge {};              // Transistor is "ON" when gatecharge > 0.0
    float m_sourcecharge {}, m_draincharge {};

    vector<Connection> gateconnections;
    vector<Connection> sourceconnections;
    vector<Connection> drainconnections;

    float gateneighborhood {}, sourceneighborhood {}, drainneighborhood {};
    float m_chargetobeon {}, m_chargetogo {};

    template <class Archive> void serialize(Archive & ar)
    {
        ar(x, y, gate, source, drain, sourcelen, drainlen, otherlen, m_area, depletion);
        ar(resist, m_gatecharge, m_sourcecharge, m_draincharge, gateneighborhood, sourceneighborhood, drainneighborhood, m_chargetobeon, m_chargetogo);
        ar(gateconnections, sourceconnections, drainconnections);
    }
};

// Gets the type of the transistor - originally for optimization purposes now more for statistical purposes
inline int Transistor::Valuate()
{
    // 1 depletion pullup
    // 2 enhancement pullup
    // 3 direct pulldown
    // 4 other
    if (depletion)
        return 1;
    if (drain == SIG_VCC)
        return 2;
    if (source == SIG_GND)
        return 3;
    return 4;
}

vector<Transistor> transistors;

void Signal::Homogenize()
{
    float charge = 0;
    for (unsigned int i = 0; i < connections.size(); i++)
    {
        if (connections[i].terminal == GATE)
            charge += transistors[connections[i].index].m_gatecharge;
        else if (connections[i].terminal == SOURCE)
            charge += transistors[connections[i].index].m_sourcecharge;
        else if (connections[i].terminal == DRAIN)
            charge += transistors[connections[i].index].m_draincharge;
    }

    for (unsigned int i = 0; i < connections.size(); i++)
    {
        if (connections[i].terminal == GATE)
            transistors[connections[i].index].m_gatecharge = charge * connections[i].proportion;
        else if (connections[i].terminal == SOURCE)
            transistors[connections[i].index].m_sourcecharge = charge * connections[i].proportion;
        else if (connections[i].terminal == DRAIN)
            transistors[connections[i].index].m_draincharge = charge * connections[i].proportion;
    }
}

void Transistor::Simulate()
{
    if (gate == SIG_GND)
        m_gatecharge = 0.0f;
    else if (gate == SIG_VCC)
        m_gatecharge = m_area;

    if (depletion)
    {
        if (drain == SIG_VCC)
        {
            float charge = m_chargetogo;

            charge /= PULLUPDEFLATOR; // pull-ups are too strong, we need to weaken them

            for (unsigned int i = 0; i < sourceconnections.size(); i++)
            {
                if (sourceconnections[i].terminal == GATE)
                    transistors[sourceconnections[i].index].m_gatecharge += charge * sourceconnections[i].proportion;
                else if (sourceconnections[i].terminal == SOURCE)
                    transistors[sourceconnections[i].index].m_sourcecharge += charge * sourceconnections[i].proportion;
                else if (sourceconnections[i].terminal == DRAIN)
                    transistors[sourceconnections[i].index].m_draincharge += charge * sourceconnections[i].proportion;
            }
        }
        else if (source == SIG_GND)
        {
            float charge = m_chargetogo;

            for (unsigned int i = 0; i < drainconnections.size(); i++)
            {
                if (drainconnections[i].terminal == GATE)
                    transistors[drainconnections[i].index].m_gatecharge -= charge * drainconnections[i].proportion;
                else if (drainconnections[i].terminal == SOURCE)
                    transistors[drainconnections[i].index].m_sourcecharge -= charge * drainconnections[i].proportion;
                else if (drainconnections[i].terminal == DRAIN)
                    transistors[drainconnections[i].index].m_draincharge -= charge * drainconnections[i].proportion;
            }
        }
        else
        {
            float sourcecharge = m_sourcecharge;
            if (sourcecharge > 0.0f)
                sourcecharge /= PULLUPDEFLATOR;

            float draincharge = m_draincharge;
            if (draincharge > 0.0f)
                draincharge /= PULLUPDEFLATOR;

            float chargetogo = ((sourcecharge - draincharge) / resist) / PULLUPDEFLATOR;
            float sign = 1.0;
            if (chargetogo < 0.0f)
            {
                sign = -1.0;
                chargetogo = -chargetogo;
            }
            if (chargetogo > MAXQUANTUM)
                chargetogo = MAXQUANTUM;
            chargetogo *= sign;

            m_sourcecharge -= chargetogo;
            m_draincharge += chargetogo;
        }
    }
    else
    {
        if (IsOn())
        {
            if (drain == SIG_VCC)
            {
                float chargetogo = m_chargetogo;
                chargetogo *= m_gatecharge / m_area;
                chargetogo /= PULLUPDEFLATOR;

                for (unsigned int i = 0; i < sourceconnections.size(); i++)
                {
                    if (sourceconnections[i].terminal == GATE)
                        transistors[sourceconnections[i].index].m_gatecharge += chargetogo * sourceconnections[i].proportion;
                    else if (sourceconnections[i].terminal == SOURCE)
                        transistors[sourceconnections[i].index].m_sourcecharge += chargetogo * sourceconnections[i].proportion;
                    else if (sourceconnections[i].terminal == DRAIN)
                        transistors[sourceconnections[i].index].m_draincharge += chargetogo * sourceconnections[i].proportion;
                }
            }
            else if (source == SIG_GND)
            {
                float chargetogo = m_chargetogo;
                chargetogo *= m_gatecharge / m_area;

                for (unsigned int i = 0; i < drainconnections.size(); i++)
                {
                    if (drainconnections[i].terminal == GATE)
                        transistors[drainconnections[i].index].m_gatecharge -= chargetogo * drainconnections[i].proportion;
                    else if (drainconnections[i].terminal == SOURCE)
                        transistors[drainconnections[i].index].m_sourcecharge -= chargetogo * drainconnections[i].proportion;
                    else if (drainconnections[i].terminal == DRAIN)
                        transistors[drainconnections[i].index].m_draincharge -= chargetogo * drainconnections[i].proportion;
                }
            }
            else
            {
                float sourcecharge = m_sourcecharge;
                if (sourcecharge > 0.0f)
                    sourcecharge /= PULLUPDEFLATOR;

                float draincharge = m_draincharge;
                if (draincharge > 0.0f)
                    draincharge /= PULLUPDEFLATOR;

                float chargetogo = ((sourcecharge - draincharge) / resist) / PULLUPDEFLATOR;
                float sign = 1.0;
                if (chargetogo < 0.0f)
                {
                    sign = -1.0;
                    chargetogo = -chargetogo;
                }
                if (chargetogo > MAXQUANTUM)
                    chargetogo = MAXQUANTUM;
                chargetogo *= m_gatecharge / m_area;
                chargetogo *= sign;

                m_sourcecharge -= chargetogo;
                m_draincharge += chargetogo;
            }
        }
    }
}

void Transistor::Normalize()
{
    if (m_gatecharge < -m_area)
        m_gatecharge = -m_area;
    else if (m_gatecharge > m_area)
        m_gatecharge = m_area;
    if (m_sourcecharge < -m_area)
        m_sourcecharge = -m_area;
    else if (m_sourcecharge > m_area)
        m_sourcecharge = m_area;
    if (m_draincharge < -m_area)
        m_draincharge = -m_area;
    else if (m_draincharge > m_area)
        m_draincharge = m_area;
}

int Pad::ReadInputStatus()
{
    int value = SIG_FLOATING;

    if (connections.size())
    {
        if (connections[0].terminal == GATE)
            value = transistors[connections[0].index].gate;
        else if (connections[0].terminal == SOURCE)
            value = transistors[connections[0].index].source;
        else if (connections[0].terminal == DRAIN)
            value = transistors[connections[0].index].drain;
    }
    if (value > SIG_VCC)
        value = SIG_FLOATING;
    return value;
}

void Pad::SetInputSignal(int signal)
{
    for (unsigned int i = 0; i < connections.size(); i++)
    {
        if (signal != SIG_FLOATING)
        {
            if (connections[i].terminal == GATE)
                transistors[connections[i].index].gate = signal;
            else if (connections[i].terminal == SOURCE)
                transistors[connections[i].index].source = signal;
            else if (connections[i].terminal == DRAIN)
                transistors[connections[i].index].drain = signal;
        }
        else
        {
            if (connections[i].terminal == GATE)
                transistors[connections[i].index].gate = origsignal;
            else if (connections[i].terminal == SOURCE)
                transistors[connections[i].index].source = origsignal;
            else if (connections[i].terminal == DRAIN)
                transistors[connections[i].index].drain = origsignal;
        }
    }
}

float Pad::ReadOutput()
{
    float shouldbe = 0, reallywas = 0;
    for (unsigned int i = 0; i < connections.size(); i++)
    {
        if (connections[i].terminal == SOURCE)
        {
            shouldbe += transistors[connections[i].index].m_area;
            reallywas += transistors[connections[i].index].m_sourcecharge;
        }
        else if (connections[i].terminal == DRAIN)
        {
            shouldbe += transistors[connections[i].index].m_area;
            reallywas += transistors[connections[i].index].m_draincharge;
        }
    }
    return reallywas / shouldbe;
}

// does not work correctly
// i.e. it cannot recoginze floating status
int Pad::ReadOutputStatus()
{
    float value = ReadOutput();
    if (value < -0.05f)
        return SIG_GND;
    else if (value > 0.05f)
        return SIG_VCC;
    return SIG_FLOATING;
}

// gets the value of 8 transistors - for listing purposes
int Z80Sim::GetRegVal(unsigned int reg[])
{
    int value = 0;
    for (int i = 7; i >= 0; i--)
        value = (value << 1) | (transistors[reg[i]].IsOn() & 1);
    return value;
}

// finds the transistor by coordinates - the coordinations must be upper - left corner ie the most top (first) and most left (second) corner
int Z80Sim::FindTransistor(unsigned int x, unsigned int y)
{
    for (unsigned int i = 0; i < transistors.size(); i++)
        if (transistors[i].x == int(x) && transistors[i].y == int(y))
            return i;
    logf((char*)(char*)"--- Error --- Transistor at %d, %d not found.\n", x, y);
    return -1;
}

/*
 * Load Z80 netlist
 */
void Z80Sim::simLoadNetlist(const char *p_z80netlist)
{
    std::ifstream is(p_z80netlist, std::ios::binary);
    cereal::BinaryInputArchive archive(is);

    // Deserialize netlist components
    archive(transistors);
    archive(signals);
    archive(pads);

    dumpPads(); // Show a list of pads and their information for debug

    sig_t1 = FindTransistor(572, 1203);
    sig_t2 = FindTransistor(831, 1895);
    sig_t3 = FindTransistor(901, 1242);
    sig_t4 = FindTransistor(876, 1259);
    sig_t5 = FindTransistor(825, 1958);
    sig_t6 = FindTransistor(921, 1211);

    sig_m1 = FindTransistor(1051, 1057);
    sig_m2 = FindTransistor(1014, 1165);
    sig_m3 = FindTransistor(1018, 1319);
    sig_m4 = FindTransistor(1027, 1300);
    sig_m5 = FindTransistor(1014, 1243);

    reg_pcl[0] = FindTransistor(1345, 3231);
    reg_pcl[1] = FindTransistor(1345, 3307);
    reg_pcl[2] = FindTransistor(1345, 3375);
    reg_pcl[3] = FindTransistor(1345, 3451);

    reg_pcl[4] = FindTransistor(1345, 3519);
    reg_pcl[5] = FindTransistor(1345, 3595);
    reg_pcl[6] = FindTransistor(1345, 3663);
    reg_pcl[7] = FindTransistor(1345, 3739);

    reg_pch[0] = FindTransistor(1345, 3827);
    reg_pch[1] = FindTransistor(1345, 3903);
    reg_pch[2] = FindTransistor(1345, 3971);
    reg_pch[3] = FindTransistor(1345, 4047);

    reg_pch[4] = FindTransistor(1345, 4115);
    reg_pch[5] = FindTransistor(1345, 4191);
    reg_pch[6] = FindTransistor(1345, 4259);
    reg_pch[7] = FindTransistor(1345, 4335);

    reg_r[0] = FindTransistor(1425, 3231);
    reg_r[1] = FindTransistor(1425, 3307);
    reg_r[2] = FindTransistor(1425, 3375);
    reg_r[3] = FindTransistor(1425, 3451);

    reg_r[4] = FindTransistor(1425, 3519);
    reg_r[5] = FindTransistor(1425, 3595);
    reg_r[6] = FindTransistor(1425, 3663);
    reg_r[7] = FindTransistor(1425, 3739);

    reg_i[0] = FindTransistor(1425, 3827);
    reg_i[1] = FindTransistor(1425, 3903);
    reg_i[2] = FindTransistor(1425, 3971);
    reg_i[3] = FindTransistor(1425, 4047);

    reg_i[4] = FindTransistor(1425, 4115);
    reg_i[5] = FindTransistor(1425, 4191);
    reg_i[6] = FindTransistor(1425, 4259);
    reg_i[7] = FindTransistor(1425, 4335);

    /////////////////////////////////////////////

    reg_z[0] = FindTransistor(1590, 3231);
    reg_z[1] = FindTransistor(1590, 3308);
    reg_z[2] = FindTransistor(1590, 3375);
    reg_z[3] = FindTransistor(1590, 3452);

    reg_z[4] = FindTransistor(1590, 3519);
    reg_z[5] = FindTransistor(1590, 3596);
    reg_z[6] = FindTransistor(1590, 3663);
    reg_z[7] = FindTransistor(1590, 3740);

    reg_w[0] = FindTransistor(1590, 3827);
    reg_w[1] = FindTransistor(1590, 3904);
    reg_w[2] = FindTransistor(1590, 3971);
    reg_w[3] = FindTransistor(1590, 4048);

    reg_w[4] = FindTransistor(1590, 4115);
    reg_w[5] = FindTransistor(1590, 4192);
    reg_w[6] = FindTransistor(1590, 4259);
    reg_w[7] = FindTransistor(1590, 4336);

    reg_spl[0] = FindTransistor(1670, 3231);
    reg_spl[1] = FindTransistor(1670, 3308);
    reg_spl[2] = FindTransistor(1670, 3375);
    reg_spl[3] = FindTransistor(1670, 3452);

    reg_spl[4] = FindTransistor(1670, 3519);
    reg_spl[5] = FindTransistor(1670, 3596);
    reg_spl[6] = FindTransistor(1670, 3663);
    reg_spl[7] = FindTransistor(1670, 3740);

    reg_sph[0] = FindTransistor(1670, 3827);
    reg_sph[1] = FindTransistor(1670, 3904);
    reg_sph[2] = FindTransistor(1670, 3971);
    reg_sph[3] = FindTransistor(1670, 4048);

    reg_sph[4] = FindTransistor(1670, 4115);
    reg_sph[5] = FindTransistor(1670, 4192);
    reg_sph[6] = FindTransistor(1670, 4259);
    reg_sph[7] = FindTransistor(1670, 4336);

    ///////////////////////////////////////////

    reg_iyl[0] = FindTransistor(1722, 3231);
    reg_iyl[1] = FindTransistor(1722, 3308);
    reg_iyl[2] = FindTransistor(1722, 3375);
    reg_iyl[3] = FindTransistor(1722, 3452);

    reg_iyl[4] = FindTransistor(1722, 3519);
    reg_iyl[5] = FindTransistor(1722, 3596);
    reg_iyl[6] = FindTransistor(1722, 3663);
    reg_iyl[7] = FindTransistor(1722, 3740);

    reg_iyh[0] = FindTransistor(1722, 3827);
    reg_iyh[1] = FindTransistor(1722, 3904);
    reg_iyh[2] = FindTransistor(1722, 3971);
    reg_iyh[3] = FindTransistor(1722, 4048);

    reg_iyh[4] = FindTransistor(1722, 4115);
    reg_iyh[5] = FindTransistor(1722, 4192);
    reg_iyh[6] = FindTransistor(1722, 4259);
    reg_iyh[7] = FindTransistor(1722, 4336);

    reg_ixl[0] = FindTransistor(1802, 3231);
    reg_ixl[1] = FindTransistor(1802, 3308);
    reg_ixl[2] = FindTransistor(1802, 3375);
    reg_ixl[3] = FindTransistor(1802, 3452);

    reg_ixl[4] = FindTransistor(1802, 3519);
    reg_ixl[5] = FindTransistor(1802, 3596);
    reg_ixl[6] = FindTransistor(1802, 3663);
    reg_ixl[7] = FindTransistor(1802, 3740);

    reg_ixh[0] = FindTransistor(1802, 3827);
    reg_ixh[1] = FindTransistor(1802, 3904);
    reg_ixh[2] = FindTransistor(1802, 3971);
    reg_ixh[3] = FindTransistor(1802, 4048);

    reg_ixh[4] = FindTransistor(1802, 4115);
    reg_ixh[5] = FindTransistor(1802, 4192);
    reg_ixh[6] = FindTransistor(1802, 4259);
    reg_ixh[7] = FindTransistor(1802, 4336);

    ///////////////////////////////////////////

    reg_e[0] = FindTransistor(1854, 3231);
    reg_e[1] = FindTransistor(1854, 3308);
    reg_e[2] = FindTransistor(1854, 3375);
    reg_e[3] = FindTransistor(1854, 3452);

    reg_e[4] = FindTransistor(1854, 3519);
    reg_e[5] = FindTransistor(1854, 3596);
    reg_e[6] = FindTransistor(1854, 3663);
    reg_e[7] = FindTransistor(1854, 3740);

    reg_d[0] = FindTransistor(1854, 3827);
    reg_d[1] = FindTransistor(1854, 3904);
    reg_d[2] = FindTransistor(1854, 3971);
    reg_d[3] = FindTransistor(1854, 4048);

    reg_d[4] = FindTransistor(1854, 4115);
    reg_d[5] = FindTransistor(1854, 4192);
    reg_d[6] = FindTransistor(1854, 4259);
    reg_d[7] = FindTransistor(1854, 4336);

    reg_e2[0] = FindTransistor(1933, 3231);
    reg_e2[1] = FindTransistor(1933, 3308);
    reg_e2[2] = FindTransistor(1933, 3375);
    reg_e2[3] = FindTransistor(1933, 3452);

    reg_e2[4] = FindTransistor(1933, 3519);
    reg_e2[5] = FindTransistor(1933, 3596);
    reg_e2[6] = FindTransistor(1933, 3663);
    reg_e2[7] = FindTransistor(1933, 3740);

    reg_d2[0] = FindTransistor(1933, 3827);
    reg_d2[1] = FindTransistor(1933, 3904);
    reg_d2[2] = FindTransistor(1933, 3971);
    reg_d2[3] = FindTransistor(1933, 4048);

    reg_d2[4] = FindTransistor(1933, 4115);
    reg_d2[5] = FindTransistor(1933, 4192);
    reg_d2[6] = FindTransistor(1933, 4259);
    reg_d2[7] = FindTransistor(1933, 4336);

    ///////////////////////////////////////////

    reg_l[0] = FindTransistor(1985, 3231);
    reg_l[1] = FindTransistor(1985, 3308);
    reg_l[2] = FindTransistor(1985, 3375);
    reg_l[3] = FindTransistor(1985, 3452);

    reg_l[4] = FindTransistor(1985, 3519);
    reg_l[5] = FindTransistor(1985, 3596);
    reg_l[6] = FindTransistor(1985, 3663);
    reg_l[7] = FindTransistor(1985, 3740);

    reg_h[0] = FindTransistor(1985, 3827);
    reg_h[1] = FindTransistor(1985, 3904);
    reg_h[2] = FindTransistor(1985, 3971);
    reg_h[3] = FindTransistor(1985, 4048);

    reg_h[4] = FindTransistor(1985, 4115);
    reg_h[5] = FindTransistor(1985, 4192);
    reg_h[6] = FindTransistor(1985, 4259);
    reg_h[7] = FindTransistor(1985, 4336);

    reg_l2[0] = FindTransistor(2065, 3231);
    reg_l2[1] = FindTransistor(2065, 3308);
    reg_l2[2] = FindTransistor(2065, 3375);
    reg_l2[3] = FindTransistor(2065, 3452);

    reg_l2[4] = FindTransistor(2065, 3519);
    reg_l2[5] = FindTransistor(2065, 3596);
    reg_l2[6] = FindTransistor(2065, 3663);
    reg_l2[7] = FindTransistor(2065, 3740);

    reg_h2[0] = FindTransistor(2065, 3827);
    reg_h2[1] = FindTransistor(2065, 3904);
    reg_h2[2] = FindTransistor(2065, 3971);
    reg_h2[3] = FindTransistor(2065, 4048);

    reg_h2[4] = FindTransistor(2065, 4115);
    reg_h2[5] = FindTransistor(2065, 4192);
    reg_h2[6] = FindTransistor(2065, 4259);
    reg_h2[7] = FindTransistor(2065, 4336);


    ///////////////////////////////////////////

    reg_c[0] = FindTransistor(2117, 3231);
    reg_c[1] = FindTransistor(2117, 3308);
    reg_c[2] = FindTransistor(2117, 3375);
    reg_c[3] = FindTransistor(2117, 3452);

    reg_c[4] = FindTransistor(2117, 3519);
    reg_c[5] = FindTransistor(2117, 3596);
    reg_c[6] = FindTransistor(2117, 3663);
    reg_c[7] = FindTransistor(2117, 3740);

    reg_b[0] = FindTransistor(2117, 3827);
    reg_b[1] = FindTransistor(2117, 3904);
    reg_b[2] = FindTransistor(2117, 3971);
    reg_b[3] = FindTransistor(2117, 4048);

    reg_b[4] = FindTransistor(2117, 4115);
    reg_b[5] = FindTransistor(2117, 4192);
    reg_b[6] = FindTransistor(2117, 4259);
    reg_b[7] = FindTransistor(2117, 4336);

    reg_c2[0] = FindTransistor(2196, 3231);
    reg_c2[1] = FindTransistor(2196, 3308);
    reg_c2[2] = FindTransistor(2196, 3375);
    reg_c2[3] = FindTransistor(2196, 3452);

    reg_c2[4] = FindTransistor(2196, 3519);
    reg_c2[5] = FindTransistor(2196, 3596);
    reg_c2[6] = FindTransistor(2196, 3663);
    reg_c2[7] = FindTransistor(2196, 3740);

    reg_b2[0] = FindTransistor(2196, 3827);
    reg_b2[1] = FindTransistor(2196, 3904);
    reg_b2[2] = FindTransistor(2196, 3971);
    reg_b2[3] = FindTransistor(2196, 4048);

    reg_b2[4] = FindTransistor(2196, 4115);
    reg_b2[5] = FindTransistor(2196, 4192);
    reg_b2[6] = FindTransistor(2196, 4259);
    reg_b2[7] = FindTransistor(2196, 4336);

    ///////////////////////////////////////////

    reg_f2[0] = FindTransistor(2248, 3231);
    reg_f2[1] = FindTransistor(2248, 3308);
    reg_f2[2] = FindTransistor(2248, 3375);
    reg_f2[3] = FindTransistor(2248, 3452);

    reg_f2[4] = FindTransistor(2248, 3519);
    reg_f2[5] = FindTransistor(2248, 3596);
    reg_f2[6] = FindTransistor(2248, 3663);
    reg_f2[7] = FindTransistor(2248, 3740);

    reg_a2[0] = FindTransistor(2248, 3827);
    reg_a2[1] = FindTransistor(2248, 3904);
    reg_a2[2] = FindTransistor(2248, 3971);
    reg_a2[3] = FindTransistor(2248, 4048);

    reg_a2[4] = FindTransistor(2248, 4115);
    reg_a2[5] = FindTransistor(2248, 4192);
    reg_a2[6] = FindTransistor(2248, 4259);
    reg_a2[7] = FindTransistor(2248, 4336);

    reg_f[0] = FindTransistor(2328, 3231);
    reg_f[1] = FindTransistor(2328, 3308);
    reg_f[2] = FindTransistor(2328, 3375);
    reg_f[3] = FindTransistor(2328, 3452);

    reg_f[4] = FindTransistor(2328, 3519);
    reg_f[5] = FindTransistor(2328, 3596);
    reg_f[6] = FindTransistor(2328, 3663);
    reg_f[7] = FindTransistor(2328, 3740);

    reg_a[0] = FindTransistor(2328, 3827);
    reg_a[1] = FindTransistor(2328, 3904);
    reg_a[2] = FindTransistor(2328, 3971);
    reg_a[3] = FindTransistor(2328, 4048);

    reg_a[4] = FindTransistor(2328, 4115);
    reg_a[5] = FindTransistor(2328, 4192);
    reg_a[6] = FindTransistor(2328, 4259);
    reg_a[7] = FindTransistor(2328, 4336);
}

void Z80Sim::dumpPads()
{
    // Sort the pads based on their pad number
    std::sort(pads.begin(), pads.end(), [](Pad i, Pad j){return i.origsignal<j.origsignal;});

    static char *d[4] = {(char*)"?", (char*)"IN", (char*)"OUT", (char*)"BIDIR"};
    logf((char*)"Pads:\n");
    for (unsigned int i=0; i<pads.size(); i++)
        logf((char*)"[%d] x,y=%d,%d  %s sig=%d\n", i, pads[i].x, pads[i].y, d[pads[i].type], pads[i].origsignal);
}

int Z80Sim::simulate()
{
    if (is_running) return 0;
    is_running = true;

    memset(memory, 0, sizeof(memory));
    memset(ports, 0, sizeof(ports));

    // Simulated Z80 program

    memory[0x00] = 0x21;
    memory[0x01] = 0x34;
    memory[0x02] = 0x12;
    memory[0x03] = 0x31;
    memory[0x04] = 0xfe;
    memory[0x05] = 0xdc;
    memory[0x06] = 0xe5;
    memory[0x07] = 0x21;
    memory[0x08] = 0x78;
    memory[0x09] = 0x56;
    memory[0x0a] = 0xe3;
    memory[0x0b] = 0xdd;
    memory[0x0c] = 0x21;
    memory[0x0d] = 0xbc;
    memory[0x0e] = 0x9a;
    memory[0x0f] = 0xdd;
    memory[0x10] = 0xe3;
    memory[0x11] = 0x76;

    // ============================= Simulation =============================
    int totcycles = 0;

    for (unsigned int i = 0; i < 1000000000; i++)
    {
        // Setting input pads
        for (unsigned int j = 0; j < pads.size(); j++)
        {
            if (pads[j].type == PAD_INPUT)
            {
                if (pads[j].origsignal == PAD__RESET)
                {
                    if (i < DIVISOR * 8)
                    {
                        pads[j].SetInputSignal(SIG_GND);
                        is_rst = true;
                    }
                    else
                    {
                        pads[j].SetInputSignal(SIG_VCC);
                        is_rst = false;
                    }
                }
                else if (pads[j].origsignal == PAD_CLK)
                {
                    int pom = i / DIVISOR;
                    if (pom & 1)
                        pads[j].SetInputSignal(SIG_VCC);
                    else
                        pads[j].SetInputSignal(SIG_GND);
                }
                else if (pads[j].origsignal == PAD__WAIT)
                {
                    pads[j].SetInputSignal(SIG_VCC);
                }
                else if (pads[j].origsignal == PAD__INT)
                {
                    pads[j].SetInputSignal(SIG_VCC);
                }
                else if (pads[j].origsignal == PAD__NMI)
                {
                    pads[j].SetInputSignal(SIG_VCC);
                }
                else if (pads[j].origsignal == PAD__BUSRQ)
                {
                    pads[j].SetInputSignal(SIG_VCC);
                }
            }
            else if (pads[j].type == PAD_BIDIRECTIONAL) // we have to pull data bus up or down when memory, I/O or interrupt instruction is read
            {
                if (is_rd) // nothing is read
                {
                    pads[j].SetInputSignal(SIG_FLOATING);
                }
                else
                {
                    if (!is_mreq) // memory is read
                    {
                        if (pads[j].origsignal == PAD_D7)
                        {
                            if (memory[lastadr] & 0x80)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D6)
                        {
                            if (memory[lastadr] & 0x40)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D5)
                        {
                            if (memory[lastadr] & 0x20)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D4)
                        {
                            if (memory[lastadr] & 0x10)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D3)
                        {
                            if (memory[lastadr] & 0x08)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D2)
                        {
                            if (memory[lastadr] & 0x04)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D1)
                        {
                            if (memory[lastadr] & 0x02)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D0)
                        {
                            if (memory[lastadr] & 0x01)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                    }
                    else if (!is_iorq) // I/O is read
                    {
                        if (pads[j].origsignal == PAD_D7)
                        {
                            if (ports[lastadr & 0xff] & 0x80)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D6)
                        {
                            if (ports[lastadr & 0xff] & 0x40)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D5)
                        {
                            if (ports[lastadr & 0xff] & 0x20)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D4)
                        {
                            if (ports[lastadr & 0xff] & 0x10)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D3)
                        {
                            if (ports[lastadr & 0xff] & 0x08)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D2)
                        {
                            if (ports[lastadr & 0xff] & 0x04)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D1)
                        {
                            if (ports[lastadr & 0xff] & 0x02)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                        else if (pads[j].origsignal == PAD_D0)
                        {
                            if (ports[lastadr & 0xff] & 0x01)
                                pads[j].SetInputSignal(SIG_VCC);
                            else
                                pads[j].SetInputSignal(SIG_GND);
                        }
                    }
                }
            }
        }
        // End of Setting input pads

        // Simulation itself
        for (unsigned int j = 0; j < transistors.size(); j++)
        {
            transistors[j].Simulate();
        }

        for (unsigned int j = 0; j < signals.size(); j++)
            if (!signals[j].ignore)
                signals[j].Homogenize();

        for (unsigned int j = 0; j < transistors.size(); j++)
            transistors[j].Normalize();

        // End of Simulation itself
        yield();
        if (!is_running) // XXX Stop()
            return 0;

        if (!(i % (DIVISOR / 5))) // writes out every 100s cycle (for output to be not too verbous)
        {
            logf((char*)"%07d: ", i);
            for (unsigned int j = 0; j < pads.size(); j++)
            {
                int pom;
                if (pads[j].type != PAD_INPUT)
                {
                    if ((pads[j].type == PAD_BIDIRECTIONAL) && (pads[j].ReadInputStatus() != SIG_FLOATING))
                        pom = pads[j].ReadInputStatus();
                    else
                        pom = pads[j].ReadOutputStatus();
                }
                else
                {
                    pom = pads[j].ReadInputStatus();
                }
                char pom2 = pom + '0';
                if (pom == 3)
                    pom2 = '.';
                else
                    pom2--;
                if (pads[j].origsignal == PAD_CLK)
                    logf((char*)"%c", pom2);
                if (pads[j].origsignal == PAD__RESET)
                    logf((char*)"%c", pom2);
                if (pads[j].origsignal == PAD__HALT)
                {
                    is_halt = (pom2 == '1');
                    logf((char*)"%c ", pom2);
                }
                if (pads[j].origsignal == PAD__M1)
                    logf((char*)"%c", pom2);
                if (pads[j].origsignal == PAD__RFSH)
                    logf((char*)"%c ", pom2);
                if (pads[j].origsignal == PAD__RD)
                {
                    is_rd = (pom2 == '1');
                    logf((char*)"%c", pom2);
                }
                if (pads[j].origsignal == PAD__WR)
                {
                    is_wr = (pom2 == '1');
                    logf((char*)"%c ", pom2);
                }
                if (pads[j].origsignal == PAD__MREQ)
                {
                    is_mreq = (pom2 == '1');
                    logf((char*)"%c", pom2);
                }
                if (pads[j].origsignal == PAD__IORQ)
                {
                    is_iorq = (pom2 == '1');
                    logf((char*)"%c ", pom2);
                }
                if (pads[j].origsignal == PAD_A15)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x8000;
                    adr |= (pom2 == '1') ? 0x8000 : 0;
                }
                if (pads[j].origsignal == PAD_A14)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x4000;
                    adr |= (pom2 == '1') ? 0x4000 : 0;
                }
                if (pads[j].origsignal == PAD_A13)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x2000;
                    adr |= (pom2 == '1') ? 0x2000 : 0;
                }
                if (pads[j].origsignal == PAD_A12)
                {
                    logf((char*)"%c ", pom2);
                    adr &= ~0x1000;
                    adr |= (pom2 == '1') ? 0x1000 : 0;
                }
                if (pads[j].origsignal == PAD_A11)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0800;
                    adr |= (pom2 == '1') ? 0x0800 : 0;
                }
                if (pads[j].origsignal == PAD_A10)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0400;
                    adr |= (pom2 == '1') ? 0x0400 : 0;
                }
                if (pads[j].origsignal == PAD_A9)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0200;
                    adr |= (pom2 == '1') ? 0x0200 : 0;
                }
                if (pads[j].origsignal == PAD_A8)
                {
                    logf((char*)"%c ", pom2);
                    adr &= ~0x0100;
                    adr |= (pom2 == '1') ? 0x0100 : 0;
                }
                if (pads[j].origsignal == PAD_A7)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0080;
                    adr |= (pom2 == '1') ? 0x0080 : 0;
                }
                if (pads[j].origsignal == PAD_A6)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0040;
                    adr |= (pom2 == '1') ? 0x0040 : 0;
                }
                if (pads[j].origsignal == PAD_A5)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0020;
                    adr |= (pom2 == '1') ? 0x0020 : 0;
                }
                if (pads[j].origsignal == PAD_A4)
                {
                    logf((char*)"%c ", pom2);
                    adr &= ~0x0010;
                    adr |= (pom2 == '1') ? 0x0010 : 0;
                }
                if (pads[j].origsignal == PAD_A3)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0008;
                    adr |= (pom2 == '1') ? 0x0008 : 0;

                }
                if (pads[j].origsignal == PAD_A2)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0004;
                    adr |= (pom2 == '1') ? 0x0004 : 0;
                }
                if (pads[j].origsignal == PAD_A1)
                {
                    logf((char*)"%c", pom2);
                    adr &= ~0x0002;
                    adr |= (pom2 == '1') ? 0x0002 : 0;
                }
                if (pads[j].origsignal == PAD_A0)
                {
                    logf((char*)"%c ", pom2);
                    adr &= ~0x0001;
                    adr |= (pom2 == '1') ? 0x0001 : 0;
                }
                if (pads[j].origsignal == PAD_D7)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x80;
                    lastdata |= (pom2 == '1') ? 0x80 : 0;
                }
                if (pads[j].origsignal == PAD_D6)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x40;
                    lastdata |= (pom2 == '1') ? 0x40 : 0;
                }
                if (pads[j].origsignal == PAD_D5)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x20;
                    lastdata |= (pom2 == '1') ? 0x20 : 0;
                }
                if (pads[j].origsignal == PAD_D4)
                {
                    logf((char*)"%c ", pom2);
                    lastdata &= ~0x10;
                    lastdata |= (pom2 == '1') ? 0x10 : 0;
                }
                if (pads[j].origsignal == PAD_D3)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x08;
                    lastdata |= (pom2 == '1') ? 0x08 : 0;
                }
                if (pads[j].origsignal == PAD_D2)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x04;
                    lastdata |= (pom2 == '1') ? 0x04 : 0;
                }
                if (pads[j].origsignal == PAD_D1)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x02;
                    lastdata |= (pom2 == '1') ? 0x02 : 0;
                }
                if (pads[j].origsignal == PAD_D0)
                {
                    logf((char*)"%c", pom2);
                    lastdata &= ~0x01;
                    lastdata |= (pom2 == '1') ? 0x01 : 0;
                }
                // End of if (pads[j].type == PAD_OUTPUT)
            }

            logf((char*)" PC:%04x", GetRegVal(reg_pch) << 8 | GetRegVal(reg_pcl));
            logf((char*)" IR:%04x", GetRegVal(reg_i) << 8 | GetRegVal(reg_r));
            logf((char*)" SP:%04x", GetRegVal(reg_sph) << 8 | GetRegVal(reg_spl));
            logf((char*)" WZ:%04x", GetRegVal(reg_w) << 8 | GetRegVal(reg_z));
            logf((char*)" IX:%04x", GetRegVal(reg_ixh) << 8 | GetRegVal(reg_ixl));
            logf((char*)" IY:%04x", GetRegVal(reg_iyh) << 8 | GetRegVal(reg_iyl));
            logf((char*)" HL:%04x", GetRegVal(reg_h) << 8 | GetRegVal(reg_l));
            logf((char*)" HL':%04x", GetRegVal(reg_h2) << 8 | GetRegVal(reg_l2));
            logf((char*)" DE:%04x", GetRegVal(reg_d) << 8 | GetRegVal(reg_e));
            logf((char*)" DE':%04x", GetRegVal(reg_d2) << 8 | GetRegVal(reg_e2));
            logf((char*)" BC:%04x", GetRegVal(reg_b) << 8 | GetRegVal(reg_c));
            logf((char*)" BC':%04x", GetRegVal(reg_b2) << 8 | GetRegVal(reg_c2));
            logf((char*)" A:%02x", GetRegVal(reg_a));
            logf((char*)" A':%02x", GetRegVal(reg_a2));
            logf((char*)" F:%c", (GetRegVal(reg_f) & 0x80) ? L'S' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x40) ? L'Z' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x20) ? L'5' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x10) ? L'H' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x08) ? L'3' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x04) ? L'V' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x02) ? L'N' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f) & 0x01) ? L'C' : L'.');
            logf((char*)" F':%c", (GetRegVal(reg_f2) & 0x80) ? L'S' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x40) ? L'Z' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x20) ? L'5' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x10) ? L'H' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x08) ? L'3' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x04) ? L'V' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x02) ? L'N' : L'.');
            logf((char*)"%c", (GetRegVal(reg_f2) & 0x01) ? L'C' : L'.');

            logf((char*)" T:%c", (transistors[sig_t1].IsOn()) ? '1' : '.');
            logf((char*)"%c", (transistors[sig_t2].IsOn()) ? '2' : '.');
            logf((char*)"%c", (transistors[sig_t3].IsOn()) ? '3' : '.');
            logf((char*)"%c", (transistors[sig_t4].IsOn()) ? '4' : '.');
            logf((char*)"%c", (transistors[sig_t5].IsOn()) ? '5' : '.');
            logf((char*)"%c", (transistors[sig_t6].IsOn()) ? '6' : '.');

            logf((char*)" M:%c", (transistors[sig_m1].IsOn()) ? '1' : '.');
            logf((char*)"%c", (transistors[sig_m2].IsOn()) ? '2' : '.');
            logf((char*)"%c", (transistors[sig_m3].IsOn()) ? '3' : '.');
            logf((char*)"%c", (transistors[sig_m4].IsOn()) ? '4' : '.');
            logf((char*)"%c", (transistors[sig_m5].IsOn()) ? '5' : '.');

            if (!is_rd && !is_mreq && transistors[sig_m1].IsOn())
                logf((char*)" ***** OPCODE FETCH: %04x[%02x]", adr, memory[adr]);

            if (!is_mreq || !is_iorq)
            {
                lastadr = adr; // gets the valid address
                if (!is_rst)
                {
                    if (!is_wr)
                    {
                        if (!is_mreq)
                        {
                            memory[lastadr] = lastdata;
                            logf((char*)" MEMORY WRITE: %04x[%02x]", lastadr, lastdata);
                        }
                        if (!is_iorq)
                        {
                            ports[lastadr & 0xff] = lastdata;
                            logf((char*)" I/O WRITE: %04x[%02x]", lastadr, lastdata);
                        }
                    }
                    if (!is_rd)
                    {
                        if (!is_mreq && !transistors[sig_m1].IsOn())
                        {
                            logf((char*)" MEMORY READ: %04x[%02x]", lastadr, memory[lastadr]);
                        }
                        if (!is_iorq)
                        {
                            logf((char*)" I/O READ: %04x[%02x]", lastadr, memory[lastadr]);
                        }
                    }
                }
            }
            logf((char*)"\n");

            if (!is_halt && !is_rst)
                outcounter++;
            else
                outcounter = 0;
            if (outcounter >= 150)
                break;
        }
        totcycles = i;
    }

    is_running = false;
    return 0;
}
