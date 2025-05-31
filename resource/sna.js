// This script sets up the configuration and runs a Sinclair ZX Spectrum SNA format file.
// Currently it is hard-coded to run one sample game, but you can change the SNA name and run
// anything, ex. have the FUSE emulator save a game in the SNA format and then load it here.
//
// To use, type the following command in the the Command window:
//    load("sna.js")
//
function ZXSpectrumSNA(filename)
{
    reset();
    if (mon.loadBin("48.rom", 0))
    {
        if (mon.patchHex("sna_patch.hex"))
        {
            if (mon.loadBin(filename, 16384-27))
            {
                mon.enabled = 0;
                mon.zx();
                run(0);
            }
            else
            {
                print("ERROR: Unable to load ", filename);
                return(0);
            }
        }
        else
        {
            print("ERROR: Unable to load sna_patch.hex\n");
            return(0);
        }
    }
    else
    {
        print("ERROR: Unable to load 48.rom\n");
        return(0);
    }
}

ZXSpectrumSNA("cookie.sna");
