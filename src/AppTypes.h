#ifndef APPTYPES_H
#define APPTYPES_H

#define APP_VERSION 109             // Application version (minor % 100)

#define USE_PERFORMANCE_SIM 1       // Use faster and optimized (but more obfuscated) simulation code
// Use optimized simulation with AVX2/x64 intrinsics. Auto-disabled on targets where the compiler does not
// advertise AVX2 (e.g. Apple Silicon macOS, x86 macOS without /arch:AVX2 or -mavx2). __AVX2__ is set by
// MSVC when /arch:AVX2 is on and by GCC/Clang when -mavx2 is on. Force off with -DUSE_AVX2_SIM=0 from the
// build system.
#ifndef USE_AVX2_SIM
#  if defined(__AVX2__)
#    define USE_AVX2_SIM 1
#  else
#    define USE_AVX2_SIM 0
#  endif
#endif
#define HAVE_PREBUILT_LAYERMAP 1    // We have extracted a fully prebuilt layermap.bin and can use it
#define FIX_Z80_LAYERMAP_TO_VISUAL_ENUM 1 // Fix to prebuilt layermap incorrectly counting nets between 1559 and 1710

// The five values below are runtime-configurable through the Edit > Settings... dialog;
// these defines are only their factory defaults, used when QSettings has no stored value.
#define HISTORY_DEPTH     1000      // Default half-cycles of waveform history retained per watched net (circular buffer)
#define SOCKET_SERVER     0         // Default for the command socket server (1 = enabled, 0 = disabled)
#define SOCKET_PORT       12345     // Default port the command socket server listens on (localhost only)
#define MCP_SERVER        0         // Default for the built-in MCP (Model Context Protocol) HTTP server
#define MCP_PORT          8765      // Default port the MCP server listens on (localhost only)

#include <stdint.h>

typedef uint16_t net_t;             // Type of an index into the net array (type of a net id value)
typedef uint16_t tran_t;            // Type of an index into the transistor array (type of a transistor id value)
typedef uint8_t  pin_t;             // Pin state: 0, 1, 2 for floating, or 3 for "no sample recorded"

#if USE_PERFORMANCE_SIM
// There is a certain performance speedup to be gained by correctly sizing (array) values
#define MAX_TRANS 8881              // Max transistor index + 1 (for this particular Z80 netlist)
#define MAX_NETS  3597              // Max net index + 1 (for this particular Z80 netlist)
#else
#define MAX_TRANS 10000             // Max number of transistors (est. for Z80)
#define MAX_NETS  4000              // Max number of nets (est. for Z80)
#endif

#if !defined (NETOP)
#define NETOP
enum class Netop : unsigned char { SetName, Rename, DeleteName, Changed };
#endif

#endif // APPTYPES_H
