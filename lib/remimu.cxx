// Wrapper for https://github.com/datadiode/Remimu to use it from vc9
// SPDX-Licence-Identifier: CC0-1.0

#define REMIMU_FUNC_VISIBILITY extern "C"
#define REMIMU_ASSERT(x)

// Allow inner blocks to hide declarations from outer blocks
#pragma warning(disable: 4456)

// Hide some headers from remimu.h
#pragma include_alias(<stdint.h>,<stddef.h>)
#pragma include_alias(<string.h>,<stddef.h>)
#pragma include_alias(<assert.h>,<stddef.h>)

// Compensate for lack of <stdint.h> in vc9
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
typedef short int16_t;
typedef int int32_t;
typedef __int64 int64_t;

// Tolerate possible loss of data due to bitness mismatch
#pragma warning(disable: 4244 4267)

// Disable all sorts of diagnostic output to avoid CRT dependencies
#include <stdio.h>
#undef printf
#define printf sizeof
#undef puts
#define puts sizeof
#undef fprintf
#define fprintf sizeof
#undef fflush
#define fflush sizeof

// Pull in CRT replacements from Yori
extern "C" {
#include <yoricrt.h>
#undef memset // because mini_memset() has non-standard signature
}

// Pull in the single-header implementation
#include "remimu.h"
