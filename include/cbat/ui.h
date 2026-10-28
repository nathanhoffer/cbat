/* cbat/ui.h - renderer-independent immediate-mode UI. MIT License.
 * Include explicitly; optional CMake CBAT_WITH_UI (default ON). Coordinates
 * are logical pixels, origin top left. Submit widgets in paint/hit-test order;
 * avoid overlapping interactive rectangles. Objects require external locking.
 * Frame/void/widget APIs latch errors: inspect cbat_ui_error after end_frame.
 * Commands are discarded on frame failure. Values successfully changed before
 * a later error remain changed. No fonts or platform event loop are included.
 */
#ifndef CBAT_UI_H
#define CBAT_UI_H
#include "cbat/base.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
typedef struct CBatUi CBatUi;
/** Logical pixel rectangle. */
typedef struct CBatUiRect { float x, y, width, height; } CBatUiRect;
/** Straight-alpha sRGB color, channels 0..255. */
typedef struct CBatUiColor { uint8_t r, g, b, a; } CBatUiColor;
/** Host text width callback; length bytes, font size in pixels. Must return finite nonnegative width. */
typedef float (*CBatUiMeasureText)(void *context, const char *text, size_t length, float font_size);
enum {
    CBAT_UI_KEY_BACKSPACE = 1u << 0, CBAT_UI_KEY_DELETE = 1u << 1,
    CBAT_UI_KEY_LEFT = 1u << 2, CBAT_UI_KEY_RIGHT = 1u << 3,
    CBAT_UI_KEY_ENTER = 1u << 4, CBAT_UI_KEY_TAB = 1u << 5,
    CBAT_UI_KEY_HOME = 1u << 6, CBAT_UI_KEY_END = 1u << 7,
    CBAT_UI_KEY_ESCAPE = 1u << 8
};
/** Snapshot once per frame. text is NUL-terminated UTF-8 (at most 31 bytes),
 * no newlines; key bits are edge/repeat events, not held-state bits. Scroll
 * deltas are pixels (positive moves toward later content). */
typedef struct CBatUiInput {
    float pointer_x, pointer_y;
    bool pointer_down;
    float scroll_x, scroll_y;
    char text[32];
    uint32_t keys_pressed;
} CBatUiInput;
typedef enum CBatUiCommandType {
    CBAT_UI_DRAW_RECT, CBAT_UI_DRAW_RECT_OUTLINE, CBAT_UI_DRAW_TEXT,
    CBAT_UI_DRAW_LINE, CBAT_UI_CLIP_PUSH, CBAT_UI_CLIP_POP
} CBatUiCommandType;
/** Flat draw command. Line endpoints: (x,y) and (x+width,y+height).
 * Clip pushes intersect the previous clip. Text is owned by UI until next
 * begin_frame/free; command pointers may move while submitting more widgets. */
typedef struct CBatUiCommand {
    CBatUiCommandType type;
    CBatUiRect rect;
    CBatUiColor color;
    float corner_radius, thickness, font_size;
    const char *text;
    size_t text_length;
} CBatUiCommand;
/** Colors and nonnegative pixel metrics (font_size > 0). */
typedef struct CBatUiStyle {
    CBatUiColor background, panel, text, accent, border;
    float padding, spacing, font_size, corner_radius;
} CBatUiStyle;
/** New UI, NULL on OOM. NULL measure uses an approximate 0.6*font_size per UTF-8 code point. */
CBAT_API CBatUi *cbat_ui_new(CBatUiMeasureText measure, void *measure_context);
/** Same with retained allocator (NULL = default); must outlive UI. */
CBAT_API CBatUi *cbat_ui_new_with(CBatUiMeasureText measure, void *measure_context, const CBatAllocator *allocator);
/** Frees UI/state/commands/text; NULL ignored. */
CBAT_API void cbat_ui_free(CBatUi *ui);
/** Starts frame, invalidating prior commands/text. NULL input = no events. Finite nonnegative viewport required. */
CBAT_API void cbat_ui_begin_frame(CBatUi *ui, const CBatUiInput *input, float width, float height);
/** Ends frame; unbalanced windows/IDs latch INVALID_ARGUMENT. Clears command list on any error. */
CBAT_API void cbat_ui_end_frame(CBatUi *ui);
/** Latched frame error (INVALID_ARGUMENT for NULL). */
CBAT_API CBatError cbat_ui_error(const CBatUi *ui);
/** Number of commands (0 for NULL). Read after successful end_frame. */
CBAT_API size_t cbat_ui_command_count(const CBatUi *ui);
/** Borrowed command or NULL if out of range. */
CBAT_API const CBatUiCommand *cbat_ui_command(const CBatUi *ui, size_t index);
/** True while a submitted text field owns focus. */
CBAT_API bool cbat_ui_wants_keyboard(const CBatUi *ui);
/** Pushes draggable window layout; click left title marker to collapse.
 * Always call window_end, even if false. rect is caller-owned and updated on drag.
 * Windows nest at most 16 deep; title namespaces child IDs. */
CBAT_API bool cbat_ui_window_begin(CBatUi *ui, const char *title, CBatUiRect *rect);
/** Pops window/clip. */
CBAT_API void cbat_ui_window_end(CBatUi *ui);
/** Repeating row of 1..64 columns; copies widths (NULL = all fill).
 * Positive = fixed pixels, zero = auto widget width, negative = equal fill.
 * height 0 uses default. New row starts below an incomplete previous row. */
CBAT_API void cbat_ui_row(CBatUi *ui, size_t columns, const float *widths, float height);
/** Places next widget beside the previous widget, overriding row layout once. */
CBAT_API void cbat_ui_same_line(CBatUi *ui);
/** Adds nonnegative vertical space. */
CBAT_API void cbat_ui_spacing(CBatUi *ui, float pixels);
/** Horizontal line occupying one row. */
CBAT_API void cbat_ui_separator(CBatUi *ui);
/** Pushes an ID namespace (64 deep maximum); labels must be unique within each namespace. */
CBAT_API void cbat_ui_push_id(CBatUi *ui, const char *id);
/** Same with an integer ID. */
CBAT_API void cbat_ui_push_id_int(CBatUi *ui, int64_t id);
/** Pops explicit ID namespace; cannot pop window's implicit namespace. */
CBAT_API void cbat_ui_pop_id(CBatUi *ui);
/** Text label. Copies text for frame lifetime. */
CBAT_API void cbat_ui_label(CBatUi *ui, const char *text);
/** True once on release inside the button after pressing inside it. */
CBAT_API bool cbat_ui_button(CBatUi *ui, const char *label);
/** True if toggled on click. */
CBAT_API bool cbat_ui_checkbox(CBatUi *ui, const char *label, bool *value);
/** True if click changed selected to value. */
CBAT_API bool cbat_ui_radio(CBatUi *ui, const char *label, int *selected, int value);
/** Drag or focused left/right keys; clamps to finite min<=max. True if changed. */
CBAT_API bool cbat_ui_slider_double(CBatUi *ui, const char *label, double *value, double min, double max);
/** Integer slider, including full int range. */
CBAT_API bool cbat_ui_slider_int(CBatUi *ui, const char *label, int *value, int min, int max);
/** Single-line UTF-8 editor, code-point cursor/delete, click positioning, home/end.
 * Enter/Escape blur; Tab cycles submitted text fields next frame. No selection/clipboard/IME preedit. */
CBAT_API bool cbat_ui_text_input(CBatUi *ui, const char *label, CBatString *text);
/** Persistent disclosure toggle (initially closed); true while expanded. */
CBAT_API bool cbat_ui_collapsing_header(CBatUi *ui, const char *label);
/** Progress indicator, fraction clamped to [0,1]. */
CBAT_API void cbat_ui_progress(CBatUi *ui, float fraction);
/** Writes default style (NULL ignored). */
CBAT_API void cbat_ui_style_default(CBatUiStyle *out);
/** Copies valid style, otherwise latches INVALID_ARGUMENT. */
CBAT_API void cbat_ui_set_style(CBatUi *ui, const CBatUiStyle *style);
CBAT_END_DECLS
#endif
