// This script sets up the configuration and boots Sinclair ZX Spectrum
// It opens a new window in which you should eventually see its logo message if you
// wait patiently - it takes approximately 20 min to get to that point when the
// simulation is running at around 4kHz.
//
mon.loadBin("48.rom", 0);
mon.rom = 16384;
mon.enabled = 0;
mon.zx();
reset();
run(0);
print("\nFor a faster simulation, remove all nets from Watchlist (Items to be tracked).");
print("It takes about 20 minutes to get to the initial screen when running at around 4kHz.");
