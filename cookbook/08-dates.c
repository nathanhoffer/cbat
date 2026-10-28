/*
 * Recipe 8: Working with dates and times
 *
 * PROBLEM
 *   Parse an ISO 8601 timestamp, shift it, count days between dates, and
 *   format the result for humans - correctly across leap years and time
 *   zone offsets.
 *
 * SOLUTION
 *   CBatDateTime holds a calendar date, time and fixed UTC offset.
 *   cbat_datetime_parse_iso / _format_iso round-trip ISO text,
 *   cbat_datetime_add_seconds does arithmetic, cbat_datetime_format takes
 *   strftime() codes.
 *
 * DISCUSSION
 *   Durations are plain seconds (as doubles), like Python's timedelta's
 *   total_seconds(). For timing code, use cbat_time_monotonic(), which is
 *   immune to clock changes; cbat_time_now() is for wall-clock time.
 */
#include <stdio.h>

#include "cbat/cbat.h"

int main(void) {
    CBatDateTime launch;
    CBatDateTime later;
    CBatDateTime new_year;
    CBatString *text = NULL;
    CBatParseInfo info;
    int64_t days = 0;
    double start = cbat_time_monotonic();

    if (cbat_datetime_parse_iso("2028-02-28T22:15:00-05:00", &launch, &info) != CBAT_OK) {
        printf("parse error at column %lu: %s\n", (unsigned long)info.column, info.message);
        return 1;
    }
    cbat_datetime_add_seconds(&launch, 3 * 3600, &later); /* crosses into a leap day */
    cbat_datetime_format_iso(&later, &text);
    printf("3 hours after launch: %s\n", cbat_string_cstr(text));
    cbat_string_free(text);

    cbat_datetime_format(&later, "%A, %B %d, %Y", &text);
    printf("which is a %s\n", cbat_string_cstr(text));
    cbat_string_free(text);

    cbat_datetime_parse_iso("2029-01-01", &new_year, NULL);
    cbat_datetime_days_between(&launch, &new_year, &days);
    printf("days from launch to 2029: %lld\n", (long long)days);
    printf("2028 is %sa leap year\n", cbat_time_is_leap_year(2028) ? "" : "not ");

    printf("this recipe took %.6f seconds\n", cbat_time_monotonic() - start);
    return later.day == 29 ? 0 : 1;
}
