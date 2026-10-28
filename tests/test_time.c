#include <string.h>

#include "cbat/test.h"
#include "cbat/time.h"

CBAT_TEST(test_clocks) {
    double now = cbat_time_now();
    double m1 = cbat_time_monotonic();
    double m2;
    CBAT_CHECK(now > 1.7e9);  /* after 2023 */
    CBAT_CHECK(now < 4.1e9);  /* before 2100 */
    cbat_time_sleep(0.02);
    m2 = cbat_time_monotonic();
    CBAT_CHECK(m2 - m1 >= 0.015);
    CBAT_CHECK(m2 - m1 < 2.0);
    cbat_time_sleep(-1); /* returns immediately */
}

CBAT_TEST(test_leap_years_and_month_lengths) {
    CBAT_CHECK(cbat_time_is_leap_year(2024));
    CBAT_CHECK(cbat_time_is_leap_year(2000));
    CBAT_CHECK(!cbat_time_is_leap_year(1900));
    CBAT_CHECK(!cbat_time_is_leap_year(2026));
    CBAT_CHECK_EQ_INT(cbat_time_days_in_month(2024, 2), 29);
    CBAT_CHECK_EQ_INT(cbat_time_days_in_month(2026, 2), 28);
    CBAT_CHECK_EQ_INT(cbat_time_days_in_month(2026, 12), 31);
    CBAT_CHECK_EQ_INT(cbat_time_days_in_month(2026, 13), 0);
}

CBAT_TEST(test_from_timestamp_utc) {
    CBatDateTime dt;
    CBAT_REQUIRE_OK(cbat_datetime_from_timestamp(0, false, &dt));
    CBAT_CHECK_EQ_INT(dt.year, 1970);
    CBAT_CHECK_EQ_INT(dt.month, 1);
    CBAT_CHECK_EQ_INT(dt.day, 1);
    CBAT_CHECK_EQ_INT(dt.weekday, 3); /* Thursday */
    /* 2026-10-15T12:34:56.5Z */
    CBAT_REQUIRE_OK(cbat_datetime_from_timestamp(1792067696.5, false, &dt));
    CBAT_CHECK_EQ_INT(dt.year, 2026);
    CBAT_CHECK_EQ_INT(dt.month, 10);
    CBAT_CHECK_EQ_INT(dt.day, 15);
    CBAT_CHECK_EQ_INT(dt.hour, 12);
    CBAT_CHECK_EQ_INT(dt.minute, 34);
    CBAT_CHECK_EQ_INT(dt.second, 56);
    CBAT_CHECK_EQ_INT(dt.nanosecond, 500000000);
    CBAT_CHECK_EQ_INT(dt.weekday, 3); /* also a Thursday */
    /* Before the epoch. */
    CBAT_REQUIRE_OK(cbat_datetime_from_timestamp(-1, false, &dt));
    CBAT_CHECK_EQ_INT(dt.year, 1969);
    CBAT_CHECK_EQ_INT(dt.month, 12);
    CBAT_CHECK_EQ_INT(dt.day, 31);
    CBAT_CHECK_EQ_INT(dt.second, 59);
    CBAT_CHECK_ERR(cbat_datetime_from_timestamp(1e300, false, &dt), CBAT_ERR_OUT_OF_RANGE);
}

CBAT_TEST(test_local_time_round_trips) {
    CBatDateTime dt;
    double back = 0;
    double ts = 1792067696.0;
    CBAT_REQUIRE_OK(cbat_datetime_from_timestamp(ts, true, &dt));
    CBAT_REQUIRE_OK(cbat_datetime_to_timestamp(&dt, &back));
    CBAT_CHECK_NEAR(back, ts, 1e-6);
    CBAT_REQUIRE_OK(cbat_datetime_now(true, &dt));
    CBAT_CHECK(cbat_datetime_is_valid(&dt));
}

CBAT_TEST(test_iso_format) {
    CBatDateTime dt;
    CBatString *s = NULL;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026; dt.month = 3; dt.day = 9; dt.hour = 7; dt.minute = 5; dt.second = 3;
    CBAT_REQUIRE_OK(cbat_datetime_format_iso(&dt, &s));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "2026-03-09T07:05:03Z");
    cbat_string_free(s);
    dt.nanosecond = 120000000;
    dt.utc_offset_minutes = -(7 * 60);
    CBAT_REQUIRE_OK(cbat_datetime_format_iso(&dt, &s));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "2026-03-09T07:05:03.12-07:00");
    cbat_string_free(s);
    dt.month = 2; dt.day = 30;
    CBAT_CHECK_ERR(cbat_datetime_format_iso(&dt, &s), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_iso_parse) {
    CBatDateTime dt;
    CBatParseInfo info;
    double ts = 0;
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15", &dt, NULL));
    CBAT_CHECK_EQ_INT(dt.day, 15);
    CBAT_CHECK_EQ_INT(dt.hour, 0);
    CBAT_CHECK_EQ_INT(dt.weekday, 3);
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15T12:34:56.5Z", &dt, NULL));
    CBAT_REQUIRE_OK(cbat_datetime_to_timestamp(&dt, &ts));
    CBAT_CHECK_NEAR(ts, 1792067696.5, 1e-6);
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15 05:34:56-07:00", &dt, NULL));
    CBAT_CHECK_EQ_INT(dt.utc_offset_minutes, -420);
    CBAT_REQUIRE_OK(cbat_datetime_to_timestamp(&dt, &ts));
    CBAT_CHECK_NEAR(ts, 1792067696.0, 1e-6);
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15T12:34+0530", &dt, NULL));
    CBAT_CHECK_EQ_INT(dt.utc_offset_minutes, 330);
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15T12:34:00+02", &dt, NULL));
    CBAT_CHECK_EQ_INT(dt.utc_offset_minutes, 120);
    CBAT_CHECK_ERR(cbat_datetime_parse_iso("2026-02-30", &dt, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_datetime_parse_iso("2026-1-5", &dt, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_datetime_parse_iso("2026-10-15T25:00", &dt, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_datetime_parse_iso("2026-10-15Tjunk", &dt, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_SIZE(info.column, 12);
    CBAT_CHECK_ERR(cbat_datetime_parse_iso("2026-10-15 trailing", &dt, &info), CBAT_ERR_PARSE);
}

CBAT_TEST(test_add_seconds_and_days_between) {
    CBatDateTime dt;
    CBatDateTime later;
    int64_t days = 0;
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2024-02-28T23:30:00.75+01:00", &dt, NULL));
    CBAT_REQUIRE_OK(cbat_datetime_add_seconds(&dt, 3600.5, &later));
    CBAT_CHECK_EQ_INT(later.month, 2);
    CBAT_CHECK_EQ_INT(later.day, 29); /* leap day */
    CBAT_CHECK_EQ_INT(later.hour, 0);
    CBAT_CHECK_EQ_INT(later.minute, 30);
    CBAT_CHECK_EQ_INT(later.second, 1);
    CBAT_CHECK_EQ_INT(later.nanosecond, 250000000);
    CBAT_CHECK_EQ_INT(later.utc_offset_minutes, 60);
    CBAT_REQUIRE_OK(cbat_datetime_add_seconds(&dt, -86400.0 * 365, &later));
    CBAT_CHECK_EQ_INT(later.year, 2023);
    CBAT_CHECK_EQ_INT(later.month, 2);
    CBAT_CHECK_EQ_INT(later.day, 28);
    CBAT_REQUIRE_OK(cbat_datetime_days_between(&later, &dt, &days));
    CBAT_CHECK_EQ_INT(days, 365);
}

CBAT_TEST(test_strftime_format) {
    CBatDateTime dt;
    CBatString *s = NULL;
    CBAT_REQUIRE_OK(cbat_datetime_parse_iso("2026-10-15T09:08:07Z", &dt, NULL));
    CBAT_REQUIRE_OK(cbat_datetime_format(&dt, "%Y/%m/%d %H-%M-%S %A %j", &s));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "2026/10/15 09-08-07 Thursday 288");
    cbat_string_free(s);
    CBAT_REQUIRE_OK(cbat_datetime_format(&dt, "", &s));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "");
    cbat_string_free(s);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_clocks),
    CBAT_TEST_ENTRY(test_leap_years_and_month_lengths),
    CBAT_TEST_ENTRY(test_from_timestamp_utc),
    CBAT_TEST_ENTRY(test_local_time_round_trips),
    CBAT_TEST_ENTRY(test_iso_format),
    CBAT_TEST_ENTRY(test_iso_parse),
    CBAT_TEST_ENTRY(test_add_seconds_and_days_between),
    CBAT_TEST_ENTRY(test_strftime_format),
};

CBAT_TEST_MAIN("time", tests)
