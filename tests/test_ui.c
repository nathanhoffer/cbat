#include "cbat/ui.h"
#include "cbat/test.h"
#include <string.h>

CBAT_TEST(test_click_once_and_owned_commands) {
    CBatUi *ui = cbat_ui_new(NULL, NULL);
    CBatUiInput input = {0};
    char label[] = "Button";
    size_t i;
    bool found = false;
    CBAT_REQUIRE_NOT_NULL(ui);
    input.pointer_x = 20; input.pointer_y = 20; input.pointer_down = true;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(!cbat_ui_button(ui, label)); cbat_ui_end_frame(ui);
    label[0] = 'X';
    for (i = 0; i < cbat_ui_command_count(ui); i++) {
        const CBatUiCommand *command = cbat_ui_command(ui, i);
        if (command->type == CBAT_UI_DRAW_TEXT) { CBAT_CHECK_EQ_STR(command->text, "Button"); found = true; }
    }
    CBAT_CHECK(found); label[0] = 'B';
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(!cbat_ui_button(ui, label)); cbat_ui_end_frame(ui);
    input.pointer_down = false;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(cbat_ui_button(ui, label)); cbat_ui_end_frame(ui);
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(!cbat_ui_button(ui, label)); cbat_ui_end_frame(ui);
    CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_OK); CBAT_CHECK_NULL(cbat_ui_command(ui, 1000));
    cbat_ui_free(ui);
}
CBAT_TEST(test_slider_and_utf8_editor) {
    CBatUi *ui = cbat_ui_new(NULL, NULL);
    CBatString *text = cbat_string_from("ab");
    CBatUiInput input = {0};
    double value = 50;
    CBAT_REQUIRE_NOT_NULL(ui); CBAT_REQUIRE_NOT_NULL(text);
    input.pointer_x = 20; input.pointer_y = 20; input.pointer_down = true;
    cbat_ui_begin_frame(ui, &input, 200, 100); (void)cbat_ui_slider_double(ui, "slider", &value, 0, 100); cbat_ui_end_frame(ui);
    input.pointer_x = 500;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(cbat_ui_slider_double(ui, "slider", &value, 0, 100)); cbat_ui_end_frame(ui);
    CBAT_CHECK_NEAR(value, 100, 0);
    input.pointer_down = false;
    cbat_ui_begin_frame(ui, &input, 200, 100); (void)cbat_ui_slider_double(ui, "slider", &value, 0, 100); cbat_ui_end_frame(ui);
    input.pointer_down = true; input.pointer_x = 100; strcpy(input.text, "\xc3\xa9");
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(cbat_ui_text_input(ui, "name", text)); cbat_ui_end_frame(ui);
    CBAT_CHECK_EQ_STR(text->data, "ab\xc3\xa9"); CBAT_CHECK(cbat_ui_wants_keyboard(ui));
    input.pointer_down = false; input.text[0] = 0; input.keys_pressed = CBAT_UI_KEY_BACKSPACE;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(cbat_ui_text_input(ui, "name", text)); cbat_ui_end_frame(ui);
    CBAT_CHECK_EQ_STR(text->data, "ab");
    input.keys_pressed = CBAT_UI_KEY_HOME | CBAT_UI_KEY_DELETE;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(cbat_ui_text_input(ui, "name", text)); cbat_ui_end_frame(ui);
    CBAT_CHECK_EQ_STR(text->data, "b");
    input.keys_pressed = CBAT_UI_KEY_ENTER;
    cbat_ui_begin_frame(ui, &input, 200, 100); CBAT_CHECK(!cbat_ui_text_input(ui, "name", text)); cbat_ui_end_frame(ui);
    CBAT_CHECK(!cbat_ui_wants_keyboard(ui)); CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_OK);
    cbat_string_free(text); cbat_ui_free(ui);
}
CBAT_TEST(test_windows_ids_layout_and_styles) {
    CBatUi *ui = cbat_ui_new(NULL, NULL);
    CBatUiInput input = {0};
    CBatUiRect rect = {0,0,200,150};
    CBatUiStyle style;
    float widths[] = {40,-1};
    bool checked = false;
    int selected = 0;
    size_t i, pushes = 0, pops = 0;
    CBAT_REQUIRE_NOT_NULL(ui); cbat_ui_style_default(&style); cbat_ui_set_style(ui, &style);
    input.pointer_down = true; input.pointer_x = 100; input.pointer_y = 20;
    cbat_ui_begin_frame(ui, &input, 400, 300);
    CBAT_CHECK(cbat_ui_window_begin(ui, "Window", &rect));
    cbat_ui_row(ui, 2, widths, 30); cbat_ui_label(ui, "a"); cbat_ui_label(ui, "b");
    cbat_ui_push_id_int(ui, 1); (void)cbat_ui_checkbox(ui, "value", &checked); cbat_ui_pop_id(ui);
    cbat_ui_push_id(ui, "second"); (void)cbat_ui_radio(ui, "value", &selected, 1); cbat_ui_pop_id(ui);
    cbat_ui_spacing(ui, 3); cbat_ui_separator(ui); cbat_ui_progress(ui, 0.5f);
    cbat_ui_window_end(ui); cbat_ui_end_frame(ui);
    CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_OK);
    for (i = 0; i < cbat_ui_command_count(ui); i++) {
        const CBatUiCommand *command = cbat_ui_command(ui, i);
        if (command->type == CBAT_UI_CLIP_PUSH) pushes++;
        if (command->type == CBAT_UI_CLIP_POP) pops++;
    }
    CBAT_CHECK_EQ_INT(pushes, pops);
    input.pointer_x = 140;
    cbat_ui_begin_frame(ui, &input, 400, 300); CBAT_CHECK(cbat_ui_window_begin(ui, "Window", &rect));
    cbat_ui_window_end(ui); cbat_ui_end_frame(ui); CBAT_CHECK_NEAR(rect.x, 40, 0);
    cbat_ui_free(ui);
}
CBAT_TEST(test_errors_oom_and_arena) {
    size_t n; bool constructed = false, framed = false;
    for (n = 0; n < 80; n++) {
        CBatUi *ui;
        cbat_test_fail_allocations_after(n); ui = cbat_ui_new(NULL, NULL); cbat_test_stop_failing_allocations();
        if (ui != NULL) constructed = true;
        cbat_ui_free(ui);
        ui = cbat_ui_new(NULL, NULL); CBAT_REQUIRE_NOT_NULL(ui);
        cbat_test_fail_allocations_after(n);
        cbat_ui_begin_frame(ui, NULL, 200, 100); cbat_ui_label(ui, "allocation test"); cbat_ui_end_frame(ui);
        cbat_test_stop_failing_allocations();
        if (cbat_ui_error(ui) == CBAT_OK) framed = true;
        else { CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_ERR_NO_MEMORY); CBAT_CHECK_EQ_INT(cbat_ui_command_count(ui), 0); }
        cbat_ui_free(ui); if (constructed && framed) break;
    }
    CBAT_CHECK(constructed && framed);
    {
        CBatArena *arena = cbat_arena_new(4096);
        CBatUi *ui;
        CBAT_REQUIRE_NOT_NULL(arena); ui = cbat_ui_new_with(NULL, NULL, cbat_arena_allocator(arena)); CBAT_REQUIRE_NOT_NULL(ui);
        cbat_ui_begin_frame(ui, NULL, 200, 100); cbat_ui_push_id(ui, "unbalanced"); cbat_ui_end_frame(ui);
        CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_ERR_INVALID_ARGUMENT); CBAT_CHECK_EQ_INT(cbat_ui_command_count(ui), 0);
        cbat_ui_begin_frame(ui, NULL, -1, 100); cbat_ui_end_frame(ui); CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_ERR_INVALID_ARGUMENT);
        cbat_ui_begin_frame(ui, NULL, 200, 100); cbat_ui_row(ui, 0, NULL, 0); cbat_ui_end_frame(ui); CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_ERR_INVALID_ARGUMENT);
        cbat_ui_free(ui); cbat_arena_free(arena);
    }
    cbat_ui_free(NULL); CBAT_CHECK_ERR(cbat_ui_error(NULL), CBAT_ERR_INVALID_ARGUMENT);
}
CBAT_TEST(test_scoped_ids_and_tab_focus) {
    CBatUi *ui = cbat_ui_new(NULL, NULL);
    CBatUiInput input = {0};
    CBatString *a = cbat_string_from("A"), *b = cbat_string_from("B");
    bool first = false, second = false;
    int frame;
    CBAT_REQUIRE_NOT_NULL(ui); CBAT_REQUIRE_NOT_NULL(a); CBAT_REQUIRE_NOT_NULL(b);
    input.pointer_x = 20; input.pointer_y = 20;
    for (frame = 0; frame < 3; frame++) {
        input.pointer_down = frame == 0;
        cbat_ui_begin_frame(ui, &input, 300, 200);
        cbat_ui_push_id(ui, "ab"); first = cbat_ui_collapsing_header(ui, "c"); cbat_ui_pop_id(ui);
        cbat_ui_push_id(ui, "a"); second = cbat_ui_collapsing_header(ui, "bc"); cbat_ui_pop_id(ui);
        cbat_ui_end_frame(ui); CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_OK);
    }
    CBAT_CHECK(first); CBAT_CHECK(!second);
    input.pointer_down = true;
    cbat_ui_begin_frame(ui, &input, 300, 200); (void)cbat_ui_text_input(ui, "a", a); (void)cbat_ui_text_input(ui, "b", b); cbat_ui_end_frame(ui);
    input.pointer_down = false; input.keys_pressed = CBAT_UI_KEY_TAB; strcpy(input.text, "x");
    cbat_ui_begin_frame(ui, &input, 300, 200); CBAT_CHECK(!cbat_ui_text_input(ui, "a", a)); CBAT_CHECK(cbat_ui_text_input(ui, "b", b)); cbat_ui_end_frame(ui);
    CBAT_CHECK_EQ_STR(a->data, "A"); CBAT_CHECK_EQ_STR(b->data, "xB");
    CBAT_CHECK(cbat_ui_wants_keyboard(ui));
    input.keys_pressed = 0; input.text[0] = 0;
    cbat_ui_begin_frame(ui, &input, 300, 200); cbat_ui_label(ui, "field removed"); cbat_ui_end_frame(ui);
    CBAT_CHECK(!cbat_ui_wants_keyboard(ui));
    cbat_string_free(a); cbat_string_free(b); cbat_ui_free(ui);
}
CBAT_TEST(test_text_edit_and_state_oom) {
    size_t n; bool succeeded = false;
    for (n = 0; n < 100; n++) {
        CBatUi *ui = cbat_ui_new(NULL, NULL);
        CBatString *text = cbat_string_from("original");
        CBatUiInput input = {0};
        CBatUiRect rect = {0,50,180,100};
        bool changed;
        CBAT_REQUIRE_NOT_NULL(ui); CBAT_REQUIRE_NOT_NULL(text);
        input.pointer_x = 150; input.pointer_y = 20; input.pointer_down = true;
        cbat_ui_begin_frame(ui, &input, 200, 200); (void)cbat_ui_text_input(ui, "edit", text); cbat_ui_end_frame(ui);
        input.pointer_down = false; strcpy(input.text, " new text");
        cbat_test_fail_allocations_after(n);
        cbat_ui_begin_frame(ui, &input, 200, 200); changed = cbat_ui_text_input(ui, "edit", text);
        if (!changed) CBAT_CHECK_EQ_STR(text->data, "original");
        (void)cbat_ui_window_begin(ui, "new state", &rect);
        (void)cbat_ui_collapsing_header(ui, "new header"); cbat_ui_window_end(ui); cbat_ui_end_frame(ui);
        cbat_test_stop_failing_allocations();
        if (cbat_ui_error(ui) == CBAT_OK) { succeeded = true; CBAT_CHECK_EQ_STR(text->data, "original new text"); }
        else { CBAT_CHECK_ERR(cbat_ui_error(ui), CBAT_ERR_NO_MEMORY); CBAT_CHECK_EQ_INT(cbat_ui_command_count(ui), 0); }
        cbat_string_free(text); cbat_ui_free(ui);
        if (succeeded) break;
    }
    CBAT_CHECK(succeeded);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_click_once_and_owned_commands), CBAT_TEST_ENTRY(test_slider_and_utf8_editor),
    CBAT_TEST_ENTRY(test_windows_ids_layout_and_styles), CBAT_TEST_ENTRY(test_errors_oom_and_arena),
    CBAT_TEST_ENTRY(test_scoped_ids_and_tab_focus), CBAT_TEST_ENTRY(test_text_edit_and_state_oom)};
CBAT_TEST_MAIN("ui", tests)
