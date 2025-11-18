// This script sets up the configuration and boots Sinclair ZX Spectrum
// It opens a new window in which you should eventually see its logo message if you
// wait patiently - it takes approximately 20 min to get to that point when the
// simulation is running at around 4kHz.
//
function ZXSpectrum()
{
   if (mon.loadBin("48.rom", 0))
   {
      mon.rom = 16384;
      mon.enabled = 0;
      mon.zx();
      reset();
      run(0);
      print("\nZX Spectrum simulation is running.")
      print("For a faster simulation, remove all nets from Watchlist and close Waveform views.");
      print("It takes about 10 minutes to get to the initial screen when running at around 10kHz.");
   }
   else
   {
      print("ERROR: Unable to load 48.rom\n");
      return(0);
   }
}

ZXSpectrum();
