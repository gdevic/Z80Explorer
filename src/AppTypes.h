#ifndef APPTYPES_H
#define APPTYPES_H

#define MAX_TRANS 9000 // Max number of transistors (est. for Z80)
#define MAX_NETS  3600 // Max number of nets (est. for Z80)

#define APP_VERSION 10 // Application version (minor % 10)

#include <stdint.h>

typedef uint16_t net_t;                 // Type of an index to the net array
typedef uint16_t tran_t;                // Type of an index to the transistor array
typedef uint8_t  pin_t;                 // Type of the pin state (0, 1; or 2 for floating)

#if !defined (NETOP)
#define NETOP
enum class Netop : unsigned char { SetName, Rename, DeleteName, Changed };
#endif

#endif // APPTYPES_H
