#ifndef CBAT_FUZZ_COMMON_H
#define CBAT_FUZZ_COMMON_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cbat/cbat.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
#define CBAT_FUZZ_CHECK(condition) do { if (!(condition)) abort(); } while (0)
#endif
