// This script sets up the configuration and boots Sinclair ZX Spectrum
// It opens a new window in which you should eventually see its logo message if you
// wait patiently - it takes approximately 20 min to get to that point when the
// simulation is running at around 4kHz.
//
monitor.loadBin("48.rom", 0);
monitor.rom = 16384;
monitor.enabled = 0;
monitor.zx();
reset();
run(0);
