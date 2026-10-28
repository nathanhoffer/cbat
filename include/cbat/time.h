/*
 * cbat/time.h - clocks, sleeping, calendar dates and ISO 8601 (cbat's
 * answer to Python's time and datetime modules).
 *
 *     double start = cbat_time_monotonic();
 *     ...work...
 *     printf("took %.3f s\n", cbat_time_monotonic() - start);
 *
 *     CBatDateTime now;
 *     cbat_datetime_now(true, &now);                  // local time
 *     CBatString *iso = NULL;
 *     cbat_datetime_format_iso(&now, &iso);           // "2026-10-15T13:45:00-07:00"
 *
 * Calendar math uses the proleptic Gregorian calendar and ignores leap
 * seconds, like Python.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_TIME_H
#define CBAT_TIME_H

#include "cbat/base.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

/** Seconds since 1970-01-01T00:00:00Z, with sub-second precision. */
CBAT_API double cbat_time_now(void);

/** Seconds from an arbitrary start; never goes backwards. For timing. */
CBAT_API double cbat_time_monotonic(void);

/** Sleeps for at least `seconds` (negative values return immediately). */
CBAT_API void cbat_time_sleep(double seconds);

CBAT_API bool cbat_time_is_leap_year(int64_t year);

/** Days in a month (1-12), or 0 for an invalid month. */
CBAT_API int cbat_time_days_in_month(int64_t year, int month);

/** A calendar date and time-of-day with a fixed UTC offset. */
typedef struct CBatDateTime {
    int64_t year;
    int month;              /* 1-12 */
    int day;                /* 1-31 */
    int hour;               /* 0-23 */
    int minute;             /* 0-59 */
    int second;             /* 0-59 */
    int nanosecond;         /* 0-999999999 */
    int utc_offset_minutes; /* e.g. -420 for UTC-07:00; 0 for UTC */
    int weekday;            /* 0 = Monday ... 6 = Sunday (filled in by cbat) */
} CBatDateTime;

/** Fields are in range (weekday is ignored). */
CBAT_API bool cbat_datetime_is_valid(const CBatDateTime *dt);

/** Breaks a timestamp into UTC (local = false) or local time (local = true). */
CBAT_API CBatError cbat_datetime_from_timestamp(double timestamp, bool local, CBatDateTime *out);

/** The current date and time. */
CBAT_API CBatError cbat_datetime_now(bool local, CBatDateTime *out);

/** Seconds since the Unix epoch for `dt` (using its UTC offset). */
CBAT_API CBatError cbat_datetime_to_timestamp(const CBatDateTime *dt, double *out);

/** dt + seconds, keeping dt's UTC offset. */
CBAT_API CBatError cbat_datetime_add_seconds(const CBatDateTime *dt, double seconds,
                                             CBatDateTime *out);

/** Whole days from a to b (b - a), counting calendar days in each own offset. */
CBAT_API CBatError cbat_datetime_days_between(const CBatDateTime *a, const CBatDateTime *b,
                                              int64_t *out);

/**
 * ISO 8601: "YYYY-MM-DDTHH:MM:SS[.fraction](Z|+HH:MM)". The fraction is
 * printed only when nanosecond != 0, without trailing zeros.
 */
CBAT_API CBatError cbat_datetime_format_iso(const CBatDateTime *dt, CBatString **out);

/**
 * Parses "YYYY-MM-DD", optionally followed by 'T' or ' ' and
 * "HH:MM[:SS[.fraction]]" and an optional "Z", "+HH:MM", "+HHMM" or "+HH".
 * No offset means UTC.
 */
CBAT_API CBatError cbat_datetime_parse_iso(const char *text, CBatDateTime *out,
                                           CBatParseInfo *info);

/**
 * Formats with strftime() codes (%Y-%m-%d %H:%M:%S, %A, %B, ...). Names of
 * days and months follow the C locale.
 */
CBAT_API CBatError cbat_datetime_format(const CBatDateTime *dt, const char *format,
                                        CBatString **out);

CBAT_END_DECLS

#endif /* CBAT_TIME_H */
