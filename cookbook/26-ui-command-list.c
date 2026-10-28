/* Recipe 26: Building a frame without a renderer.
 * PROBLEM: Use the same widgets in a desktop, mobile or test host.
 * SOLUTION: Supply a frame snapshot, submit widgets, consume draw commands.
 * DISCUSSION: This headless recipe prints counts; docs/examples/ui_sdl.c draws
 * them with SDL2. A production host supplies font measurement and real events.
 * Always pair window_begin/end, including when the window is collapsed.
 */
#include "cbat/ui.h"
#include <stdio.h>

int main(void) {
    CBatUi *ui = cbat_ui_new(NULL, NULL);
    CBatUiInput input = {0};
    CBatUiRect window = {20,20,320,300};
    CBatString *name = cbat_string_from("Ada");
    bool enabled = true;
    double volume = 0.5;
    CBatError err;
    size_t i, text_commands = 0;
    if (ui == NULL || name == NULL) { cbat_ui_free(ui); cbat_string_free(name); return 1; }
    cbat_ui_begin_frame(ui, &input, 640, 480);
    if (cbat_ui_window_begin(ui, "Settings", &window)) {
        cbat_ui_label(ui, "Name"); (void)cbat_ui_text_input(ui, "name", name);
        (void)cbat_ui_checkbox(ui, "Notifications", &enabled);
        (void)cbat_ui_slider_double(ui, "Volume", &volume, 0, 1);
        cbat_ui_progress(ui, (float)volume);
        (void)cbat_ui_button(ui, "Apply");
    }
    cbat_ui_window_end(ui); cbat_ui_end_frame(ui); err = cbat_ui_error(ui);
    if (err == CBAT_OK) {
        for (i = 0; i < cbat_ui_command_count(ui); i++)
            if (cbat_ui_command(ui, i)->type == CBAT_UI_DRAW_TEXT) text_commands++;
        printf("Frame: %lu commands, %lu text runs\n", (unsigned long)cbat_ui_command_count(ui), (unsigned long)text_commands);
    } else fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_ui_free(ui); cbat_string_free(name);
    return err == CBAT_OK ? 0 : 1;
}
