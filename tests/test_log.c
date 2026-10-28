#include <string.h>

#include "cbat/log.h"
#include "cbat/string.h"
#include "cbat/test.h"

typedef struct Captured {
    int count;
    CBatLogLevel level;
    int line;
    char message[2048];
    size_t length;
} Captured;

static void capture(CBatLogLevel level, const char *file, int line, const char *message,
                    void *user_data) {
    Captured *c = (Captured *)user_data;
    CBAT_UNUSED(file);
    c->count++;
    c->level = level;
    c->line = line;
    c->length = strlen(message);
    strncpy(c->message, message, sizeof(c->message) - 1);
    c->message[sizeof(c->message) - 1] = '\0';
}

CBAT_TEST(test_level_filter_and_handler) {
    Captured c;
    memset(&c, 0, sizeof(c));
    cbat_log_set_handler(capture, &c);
    cbat_log_set_level(CBAT_LOG_WARNING);
    CBAT_LOG_INFO("dropped %d", 1);
    CBAT_CHECK_EQ_INT(c.count, 0);
    CBAT_LOG_WARNING("kept %s", "warning");
    CBAT_CHECK_EQ_INT(c.count, 1);
    CBAT_CHECK_EQ_INT(c.level, CBAT_LOG_WARNING);
    CBAT_CHECK_EQ_STR(c.message, "kept warning");
    CBAT_CHECK(c.line > 0);
    CBAT_LOG_ERROR("error");
    CBAT_CHECK_EQ_INT(c.count, 2);
    cbat_log_set_level(CBAT_LOG_OFF);
    CBAT_LOG_ERROR("nothing");
    CBAT_CHECK_EQ_INT(c.count, 2);
    CBAT_CHECK_EQ_INT(cbat_log_get_level(), CBAT_LOG_OFF);
    cbat_log_set_level(CBAT_LOG_INFO);
    cbat_log_set_handler(NULL, NULL);
}

CBAT_TEST(test_long_message_is_not_truncated) {
    Captured c;
    CBatString *big = cbat_string_repeat("x", 1500);
    CBAT_REQUIRE_NOT_NULL(big);
    memset(&c, 0, sizeof(c));
    cbat_log_set_handler(capture, &c);
    CBAT_LOG_INFO("%s", cbat_string_cstr(big));
    CBAT_CHECK_EQ_SIZE(c.length, 1500);
    /* Out of memory: falls back to the truncated stack copy. */
    cbat_test_fail_allocations_after(0);
    CBAT_LOG_INFO("%s", cbat_string_cstr(big));
    cbat_test_stop_failing_allocations();
    CBAT_CHECK_EQ_SIZE(c.length, 1023);
    cbat_log_set_handler(NULL, NULL);
    cbat_string_free(big);
}

CBAT_TEST(test_default_output_format) {
    FILE *out = tmpfile();
    char line[256];
    CBAT_REQUIRE_NOT_NULL(out);
    cbat_log_set_output(out);
    cbat_log_set_show_location(true);
    CBAT_LOG_WARNING("disk %d%% full", 91);
    cbat_log_set_show_location(false);
    cbat_log_set_show_time(true);
    CBAT_LOG_INFO("with time");
    cbat_log_set_show_time(false);
    cbat_log_set_output(NULL);
    rewind(out);
    CBAT_REQUIRE_NOT_NULL(fgets(line, sizeof(line), out));
    CBAT_CHECK(strncmp(line, "WARNING: test_log.c:", 20) == 0);
    CBAT_CHECK(strstr(line, "disk 91% full\n") != NULL);
    CBAT_REQUIRE_NOT_NULL(fgets(line, sizeof(line), out));
    CBAT_CHECK(line[4] == '-' && line[10] == ' '); /* "YYYY-MM-DD hh:..." */
    CBAT_CHECK(strstr(line, "INFO: with time\n") != NULL);
    fclose(out);
}

CBAT_TEST(test_level_names_and_parsing) {
    CBatLogLevel level = CBAT_LOG_DEBUG;
    CBAT_CHECK_EQ_STR(cbat_log_level_name(CBAT_LOG_ERROR), "ERROR");
    CBAT_CHECK_EQ_STR(cbat_log_level_name((CBatLogLevel)42), "UNKNOWN");
    CBAT_CHECK_OK(cbat_log_parse_level("Warn", &level));
    CBAT_CHECK_EQ_INT(level, CBAT_LOG_WARNING);
    CBAT_CHECK_OK(cbat_log_parse_level("DEBUG", &level));
    CBAT_CHECK_EQ_INT(level, CBAT_LOG_DEBUG);
    CBAT_CHECK_ERR(cbat_log_parse_level("loud", &level), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_log_parse_level(NULL, &level), CBAT_ERR_INVALID_ARGUMENT);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_level_filter_and_handler),
    CBAT_TEST_ENTRY(test_long_message_is_not_truncated),
    CBAT_TEST_ENTRY(test_default_output_format),
    CBAT_TEST_ENTRY(test_level_names_and_parsing),
};

CBAT_TEST_MAIN("log", tests)
