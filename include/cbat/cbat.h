/*
 * cbat/cbat.h - includes every tier-1 cbat module.
 * Objects have no internal locks. Configure globals before starting threads
 * and synchronize shared mutable state; see docs/DESIGN.md (Thread safety).
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_CBAT_H
#define CBAT_CBAT_H

#include "cbat/base.h"
#include "cbat/test.h"
#include "cbat/sort.h"
#include "cbat/list.h"
#include "cbat/hash.h"
#include "cbat/string.h"
#include "cbat/dict.h"
#include "cbat/map.h"
#include "cbat/set.h"
#include "cbat/deque.h"
#include "cbat/path.h"
#include "cbat/fs.h"
#include "cbat/file.h"
#include "cbat/encoding.h"
#include "cbat/csv.h"
#include "cbat/json.h"
#include "cbat/random.h"
#include "cbat/math.h"
#include "cbat/stats.h"
#include "cbat/time.h"
#include "cbat/log.h"
#include "cbat/args.h"

#endif /* CBAT_CBAT_H */
