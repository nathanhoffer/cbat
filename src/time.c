/*
 * cbat/time.c - clocks and calendar arithmetic.
 * Civil-date conversions use Howard Hinnant's public-domain algorithms
 * (days_from_civil / civil_from_days), valid far beyond any time_t range.
 * Part of cbat (C Batteries Included). MIT License.
 */
/* cbat:feature-macros-begin */
#if !defined(_WIN32)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif
/* cbat:feature-macros-end */

#include "cbat/time.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0602 /* Windows 8+: GetSystemTimePreciseAsFileTime */
#  endif
#  include <windows.h>
#endif

/* ---- Clocks ------------------------------------------------------------- */

#ifdef _WIN32

double cbat_time_now(void) {
    FILETIME ft;
    ULARGE_INTEGER ticks;
    GetSystemTimePreciseAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    /* 100 ns ticks since 1601-01-01; the Unix epoch is 11644473600 s later. */
    return (double)(ticks.QuadPart - 116444736000000000ull) / 1e7;
}

double cbat_time_monotonic(void) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

void cbat_time_sleep(double seconds) {
    if (seconds > 0) {
        double ms = ceil(seconds * 1000.0);
        Sleep(ms > 4294967294.0 ? 4294967294u : (DWORD)ms);
    }
}

static bool cbat__time_local_tm(time_t t, struct tm *out) {
    return localtime_s(out, &t) == 0;
}

#else

double cbat_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

double cbat_time_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void cbat_time_sleep(double seconds) {
    struct timespec request;
    struct timespec remaining;
    if (!(seconds > 0)) {
        return;
    }
    request.tv_sec = (time_t)seconds;
    request.tv_nsec = (long)((seconds - (double)request.tv_sec) * 1e9);
    while (nanosleep(&request, &remaining) != 0) {
        request = remaining; /* interrupted by a signal: keep sleeping */
    }
}

static bool cbat__time_local_tm(time_t t, struct tm *out) {
    return localtime_r(&t, out) != NULL;
}

#endif

/* ---- Calendar math ------------------------------------------------------ */

bool cbat_time_is_leap_year(int64_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int cbat_time_days_in_month(int64_t year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && cbat_time_is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

/* Days since 1970-01-01 for a proleptic Gregorian date. */
static int64_t cbat__days_from_civil(int64_t y, int m, int d) {
    int64_t era;
    int64_t yoe;
    int64_t doy;
    int64_t doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void cbat__civil_from_days(int64_t z, int64_t *y, int *m, int *d) {
    int64_t era;
    int64_t doe;
    int64_t yoe;
    int64_t doy;
    int64_t mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

/* 1970-01-01 was a Thursday (weekday 3 with Monday = 0). */
static int cbat__weekday(int64_t days) {
    int64_t w = (days + 3) % 7;
    return (int)(w < 0 ? w + 7 : w);
}

bool cbat_datetime_is_valid(const CBatDateTime *dt) {
    return dt != NULL && dt->month >= 1 && dt->month <= 12 && dt->day >= 1 &&
           dt->day <= cbat_time_days_in_month(dt->year, dt->month) && dt->hour >= 0 &&
           dt->hour <= 23 && dt->minute >= 0 && dt->minute <= 59 && dt->second >= 0 &&
           dt->second <= 59 && dt->nanosecond >= 0 && dt->nanosecond <= 999999999 &&
           dt->utc_offset_minutes > -24 * 60 && dt->utc_offset_minutes < 24 * 60;
}

/* Fills every field from whole seconds since the epoch (already shifted
   into the target offset) plus nanoseconds. */
static void cbat__datetime_fill(int64_t local_seconds, int nanosecond, int offset_minutes,
                                CBatDateTime *out) {
    int64_t days = local_seconds >= 0 ? local_seconds / 86400
                                      : -((-local_seconds + 86399) / 86400);
    int64_t rest = local_seconds - days * 86400;
    cbat__civil_from_days(days, &out->year, &out->month, &out->day);
    out->hour = (int)(rest / 3600);
    out->minute = (int)(rest % 3600 / 60);
    out->second = (int)(rest % 60);
    out->nanosecond = nanosecond;
    out->utc_offset_minutes = offset_minutes;
    out->weekday = cbat__weekday(days);
}

/* Splits a timestamp into whole seconds (floor) and nanoseconds. */
static CBatError cbat__time_split(double timestamp, int64_t *seconds, int *nanosecond) {
    double whole;
    double fraction;
    long ns;
    if (timestamp != timestamp || timestamp >= 9.2e15 || timestamp <= -9.2e15) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    whole = floor(timestamp);
    fraction = timestamp - whole;
    *seconds = (int64_t)whole;
    ns = (long)floor(fraction * 1e9 + 0.5);
    if (ns >= 1000000000L) {
        ns -= 1000000000L;
        (*seconds)++;
    }
    *nanosecond = (int)ns;
    return CBAT_OK;
}

CBatError cbat_datetime_from_timestamp(double timestamp, bool local, CBatDateTime *out) {
    int64_t seconds;
    int nanosecond;
    int offset = 0;
    CBatError err;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__time_split(timestamp, &seconds, &nanosecond);
    if (err != CBAT_OK) {
        return err;
    }
    if (local) {
        struct tm tm_local;
        int64_t local_seconds;
        if ((int64_t)(time_t)seconds != seconds || !cbat__time_local_tm((time_t)seconds, &tm_local)) {
            return CBAT_ERR_OUT_OF_RANGE;
        }
        local_seconds = cbat__days_from_civil((int64_t)tm_local.tm_year + 1900, tm_local.tm_mon + 1,
                                              tm_local.tm_mday) * 86400 +
                        tm_local.tm_hour * 3600 + tm_local.tm_min * 60 + tm_local.tm_sec;
        offset = (int)((local_seconds - seconds) / 60);
    }
    cbat__datetime_fill(seconds + (int64_t)offset * 60, nanosecond, offset, out);
    return CBAT_OK;
}

CBatError cbat_datetime_now(bool local, CBatDateTime *out) {
    return cbat_datetime_from_timestamp(cbat_time_now(), local, out);
}

/* Whole seconds since the epoch in UTC. */
static int64_t cbat__datetime_epoch_seconds(const CBatDateTime *dt) {
    return cbat__days_from_civil(dt->year, dt->month, dt->day) * 86400 + dt->hour * 3600 +
           dt->minute * 60 + dt->second - (int64_t)dt->utc_offset_minutes * 60;
}

CBatError cbat_datetime_to_timestamp(const CBatDateTime *dt, double *out) {
    if (out == NULL || !cbat_datetime_is_valid(dt)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    *out = (double)cbat__datetime_epoch_seconds(dt) + (double)dt->nanosecond / 1e9;
    return CBAT_OK;
}

CBatError cbat_datetime_add_seconds(const CBatDateTime *dt, double seconds, CBatDateTime *out) {
    int64_t whole;
    int nanosecond;
    int64_t total_ns;
    int64_t base;
    CBatError err;
    if (out == NULL || !cbat_datetime_is_valid(dt)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__time_split(seconds, &whole, &nanosecond);
    if (err != CBAT_OK) {
        return err;
    }
    /* Integer arithmetic keeps nanoseconds exact. */
    total_ns = (int64_t)dt->nanosecond + nanosecond;
    base = cbat__datetime_epoch_seconds(dt) + whole + total_ns / 1000000000;
    cbat__datetime_fill(base + (int64_t)dt->utc_offset_minutes * 60, (int)(total_ns % 1000000000),
                        dt->utc_offset_minutes, out);
    return CBAT_OK;
}

CBatError cbat_datetime_days_between(const CBatDateTime *a, const CBatDateTime *b, int64_t *out) {
    if (out == NULL || !cbat_datetime_is_valid(a) || !cbat_datetime_is_valid(b)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    *out = cbat__days_from_civil(b->year, b->month, b->day) -
           cbat__days_from_civil(a->year, a->month, a->day);
    return CBAT_OK;
}

/* ---- ISO 8601 ----------------------------------------------------------- */

CBatError cbat_datetime_format_iso(const CBatDateTime *dt, CBatString **out) {
    CBatString *s;
    CBatError err;
    if (out == NULL || !cbat_datetime_is_valid(dt)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    s = cbat_string_from_format("%04lld-%02d-%02dT%02d:%02d:%02d", (long long)dt->year,
                                dt->month, dt->day, dt->hour, dt->minute, dt->second);
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = CBAT_OK;
    if (dt->nanosecond != 0) {
        char digits[16];
        int length = 9;
        snprintf(digits, sizeof(digits), "%09d", dt->nanosecond);
        while (length > 1 && digits[length - 1] == '0') {
            length--;
        }
        digits[length] = '\0';
        err = cbat_string_append_format(s, ".%s", digits);
    }
    if (err == CBAT_OK) {
        if (dt->utc_offset_minutes == 0) {
            err = cbat_string_append_char(s, 'Z');
        } else {
            int offset = dt->utc_offset_minutes;
            char sign = offset < 0 ? '-' : '+';
            if (offset < 0) {
                offset = -offset;
            }
            err = cbat_string_append_format(s, "%c%02d:%02d", sign, offset / 60, offset % 60);
        }
    }
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return err;
    }
    *out = s;
    return CBAT_OK;
}

/* Reads exactly `count` digits. */
static bool cbat__iso_digits(const char **p, int count, int *out) {
    int value = 0;
    int i;
    for (i = 0; i < count; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    *p += count;
    *out = value;
    return true;
}

static CBatError cbat__iso_fail(const char *start, const char *at, const char *message,
                                CBatParseInfo *info) {
    if (info != NULL) {
        info->line = 1;
        info->offset = (size_t)(at - start);
        info->column = info->offset + 1;
        info->message = message;
    }
    return CBAT_ERR_PARSE;
}

CBatError cbat_datetime_parse_iso(const char *text, CBatDateTime *out, CBatParseInfo *info) {
    const char *p = text;
    CBatDateTime dt;
    int year;
    if (text == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    memset(&dt, 0, sizeof(dt));
    if (!cbat__iso_digits(&p, 4, &year) || *p++ != '-' || !cbat__iso_digits(&p, 2, &dt.month) ||
        *p++ != '-' || !cbat__iso_digits(&p, 2, &dt.day)) {
        return cbat__iso_fail(text, p, "expected a date like YYYY-MM-DD", info);
    }
    dt.year = year;
    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        if (!cbat__iso_digits(&p, 2, &dt.hour) || *p++ != ':' ||
            !cbat__iso_digits(&p, 2, &dt.minute)) {
            return cbat__iso_fail(text, p, "expected a time like HH:MM", info);
        }
        if (*p == ':') {
            p++;
            if (!cbat__iso_digits(&p, 2, &dt.second)) {
                return cbat__iso_fail(text, p, "expected two digits of seconds", info);
            }
            if (*p == '.' || *p == ',') {
                int scale = 100000000;
                p++;
                if (*p < '0' || *p > '9') {
                    return cbat__iso_fail(text, p, "expected digits after the decimal point", info);
                }
                while (*p >= '0' && *p <= '9') {
                    dt.nanosecond += (*p - '0') * scale; /* extra digits beyond ns are ignored */
                    scale /= 10;
                    p++;
                }
            }
        }
        if (*p == 'Z' || *p == 'z') {
            p++;
        } else if (*p == '+' || *p == '-') {
            int sign = *p == '-' ? -1 : 1;
            int hours;
            int minutes = 0;
            p++;
            if (!cbat__iso_digits(&p, 2, &hours)) {
                return cbat__iso_fail(text, p, "expected a UTC offset like +HH:MM", info);
            }
            if (*p == ':') {
                p++;
            }
            if (*p >= '0' && *p <= '9' && !cbat__iso_digits(&p, 2, &minutes)) {
                return cbat__iso_fail(text, p, "expected two digits of offset minutes", info);
            }
            dt.utc_offset_minutes = sign * (hours * 60 + minutes);
        }
    }
    if (*p != '\0') {
        return cbat__iso_fail(text, p, "unexpected text after the date/time", info);
    }
    if (!cbat_datetime_is_valid(&dt)) {
        return cbat__iso_fail(text, text, "date or time field out of range", info);
    }
    dt.weekday = cbat__weekday(cbat__days_from_civil(dt.year, dt.month, dt.day));
    *out = dt;
    return CBAT_OK;
}

CBatError cbat_datetime_format(const CBatDateTime *dt, const char *format, CBatString **out) {
    struct tm tm_value;
    CBatString *s;
    size_t capacity = 64;
    if (out == NULL || format == NULL || !cbat_datetime_is_valid(dt)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (dt->year < -2000000000LL + 1900 || dt->year > 2000000000LL) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = (int)(dt->year - 1900);
    tm_value.tm_mon = dt->month - 1;
    tm_value.tm_mday = dt->day;
    tm_value.tm_hour = dt->hour;
    tm_value.tm_min = dt->minute;
    tm_value.tm_sec = dt->second;
    tm_value.tm_wday = (cbat__weekday(cbat__days_from_civil(dt->year, dt->month, dt->day)) + 1) % 7;
    tm_value.tm_yday = (int)(cbat__days_from_civil(dt->year, dt->month, dt->day) -
                             cbat__days_from_civil(dt->year, 1, 1));
    tm_value.tm_isdst = -1;
    s = cbat_string_new();
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    /* strftime returns 0 both for "too small" and for empty output; grow a
       few times before concluding the output is genuinely empty. */
    for (;;) {
        size_t written;
        if (cbat_string_reserve(s, capacity) != CBAT_OK) {
            cbat_string_free(s);
            return CBAT_ERR_NO_MEMORY;
        }
        written = strftime(s->data, s->capacity, format, &tm_value);
        if (written > 0 || format[0] == '\0' || capacity >= 65536) {
            s->length = written;
            s->data[written] = '\0';
            break;
        }
        capacity *= 4;
    }
    *out = s;
    return CBAT_OK;
}
