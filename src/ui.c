/* Renderer-independent immediate-mode UI. Part of cbat. MIT License. */
#include "cbat/ui.h"
#include "cbat/hash.h"
#include "cbat/map.h"
#include <math.h>
#include <string.h>

typedef struct cbat__ui_state { bool open; float scroll_x, scroll_y, max_x, max_y; size_t cursor; } cbat__ui_state;
typedef struct cbat__ui_layout {
    CBatUiRect bounds, clip, last;
    float x, y, row_y, row_height, content_right, content_bottom;
    float widths[64]; size_t columns, column, id_base;
    uint64_t window;
    bool same, has_last, hidden;
} cbat__ui_layout;
struct CBatUi {
    const CBatAllocator *allocator;
    CBatUiMeasureText measure; void *measure_context;
    CBatUiStyle style; CBatUiInput input;
    CBatList *commands, *strings, *fields, *previous_fields;
    CBatMap *states;
    cbat__ui_layout layouts[17]; size_t depth;
    uint64_t ids[65]; size_t id_depth;
    uint64_t active, focus;
    float drag_x, drag_y;
    bool frame, previous_down, pressed, released, focus_seen, text_focus;
    CBatError error;
};

static void cbat__ui_fail(CBatUi *ui, CBatError error) { if (ui != NULL && ui->error == CBAT_OK) ui->error = error; }
static bool cbat__ui_live(CBatUi *ui) {
    if (ui == NULL) return false;
    if (!ui->frame) cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT);
    return ui->frame && ui->error == CBAT_OK;
}
static CBatUiRect cbat__ui_rect(float x, float y, float width, float height) {
    CBatUiRect r; r.x = x; r.y = y; r.width = width; r.height = height; return r;
}
static bool cbat__ui_valid_rect(CBatUiRect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height) && r.width >= 0 && r.height >= 0 &&
        isfinite(r.x + r.width) && isfinite(r.y + r.height);
}
static CBatUiRect cbat__ui_intersect(CBatUiRect a, CBatUiRect b) {
    float x = fmaxf(a.x, b.x), y = fmaxf(a.y, b.y);
    return cbat__ui_rect(x, y, fmaxf(0, fminf(a.x+a.width, b.x+b.width)-x), fmaxf(0, fminf(a.y+a.height, b.y+b.height)-y));
}
static bool cbat__ui_inside(CBatUi *ui, CBatUiRect rect) {
    return ui->input.pointer_x >= rect.x && ui->input.pointer_y >= rect.y &&
        ui->input.pointer_x < rect.x + rect.width && ui->input.pointer_y < rect.y + rect.height;
}
static bool cbat__ui_hover(CBatUi *ui, CBatUiRect rect) {
    return !ui->layouts[ui->depth].hidden && cbat__ui_inside(ui, cbat__ui_intersect(rect, ui->layouts[ui->depth].clip));
}
static uint64_t cbat__ui_id(CBatUi *ui, const char *label) {
    uint64_t hash = ui->ids[ui->id_depth];
    uint64_t length = (uint64_t)strlen(label);
    unsigned i;
    const unsigned char *p = (const unsigned char *)label;
    /* Frame each namespace component so ("ab","c") != ("a","bc"). */
    hash ^= 0xff; hash *= UINT64_C(1099511628211);
    for (i = 0; i < 8; i++) { hash ^= (length >> (8*i)) & 255; hash *= UINT64_C(1099511628211); }
    for (; *p != 0; p++) { hash ^= *p; hash *= UINT64_C(1099511628211); }
    return hash == 0 ? 1 : hash;
}
static cbat__ui_state *cbat__ui_state_get(CBatUi *ui, uint64_t id) {
    cbat__ui_state *state = (cbat__ui_state *)cbat_map_get(ui->states, &id);
    if (state == NULL) {
        cbat__ui_state initial; CBatError err;
        memset(&initial, 0, sizeof(initial));
        err = cbat_map_set(ui->states, &id, &initial);
        if (err != CBAT_OK) { cbat__ui_fail(ui, err); return NULL; }
        state = (cbat__ui_state *)cbat_map_get(ui->states, &id);
    }
    return state;
}
static void cbat__ui_draw(CBatUi *ui, CBatUiCommandType type, CBatUiRect rect, CBatUiColor color) {
    CBatUiCommand command; CBatError err;
    if (ui->error != CBAT_OK) return;
    if (!isfinite(rect.x) || !isfinite(rect.y) || !isfinite(rect.width) || !isfinite(rect.height) ||
        !isfinite(rect.x+rect.width) || !isfinite(rect.y+rect.height)) { cbat__ui_fail(ui, CBAT_ERR_OUT_OF_RANGE); return; }
    if (!cbat__ui_valid_rect(rect) && type != CBAT_UI_DRAW_LINE) { cbat__ui_fail(ui, CBAT_ERR_OUT_OF_RANGE); return; }
    memset(&command, 0, sizeof(command)); command.type = type; command.rect = rect; command.color = color;
    command.corner_radius = ui->style.corner_radius; command.thickness = 1; command.font_size = ui->style.font_size;
    err = cbat_list_push(ui->commands, &command); cbat__ui_fail(ui, err);
}
static float cbat__ui_measure(CBatUi *ui, const char *text, size_t length) {
    float width;
    if (ui->measure != NULL) width = ui->measure(ui->measure_context, text, length, ui->style.font_size);
    else {
        size_t i, count = 0;
        for (i = 0; i < length; i++) if (((unsigned char)text[i] & 0xc0) != 0x80) count++;
        width = (float)count * ui->style.font_size * 0.6f;
    }
    if (!isfinite(width) || width < 0) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return 0; }
    return width;
}
static void cbat__ui_text(CBatUi *ui, CBatUiRect rect, const char *text) {
    CBatString *copy; CBatError err;
    if (ui->error != CBAT_OK) return;
    copy = cbat_string_new_with(ui->allocator);
    if (copy == NULL) { cbat__ui_fail(ui, CBAT_ERR_NO_MEMORY); return; }
    err = cbat_string_append(copy, text);
    if (err == CBAT_OK) err = cbat_list_push(ui->strings, &copy);
    if (err != CBAT_OK) { cbat_string_free(copy); cbat__ui_fail(ui, err); return; }
    cbat__ui_draw(ui, CBAT_UI_DRAW_TEXT, rect, ui->style.text);
    if (ui->error == CBAT_OK) {
        CBatUiCommand *command = (CBatUiCommand *)cbat_list_get(ui->commands, ui->commands->length - 1);
        command->text = copy->data; command->text_length = copy->length;
    }
}
static float cbat__ui_height(CBatUi *ui) { return ui->style.font_size + 2*ui->style.padding; }
static void cbat__ui_layout_init(CBatUi *ui, cbat__ui_layout *layout, CBatUiRect bounds, CBatUiRect clip) {
    memset(layout, 0, sizeof(*layout)); layout->bounds = bounds; layout->clip = clip;
    layout->x = bounds.x; layout->y = bounds.y; layout->row_y = bounds.y;
    layout->columns = 1; layout->widths[0] = -1; layout->row_height = cbat__ui_height(ui);
    layout->content_right = bounds.x; layout->content_bottom = bounds.y;
}
static CBatUiRect cbat__ui_next(CBatUi *ui, const char *label) {
    cbat__ui_layout *layout = &ui->layouts[ui->depth];
    CBatUiRect rect;
    float width, available = layout->bounds.width, fixed = 0;
    size_t i, fill = 0;
    if (layout->same && layout->has_last) {
        float x = layout->last.x + layout->last.width + ui->style.spacing;
        rect = cbat__ui_rect(x, layout->last.y, fmaxf(0, layout->bounds.x + available - x), layout->last.height);
        layout->same = false;
    } else {
        if (layout->column == 0) { layout->x = layout->bounds.x; layout->row_y = layout->y; }
        for (i = 0; i < layout->columns; i++) {
            if (layout->widths[i] < 0) fill++;
            else if (layout->widths[i] > 0) fixed += layout->widths[i];
        }
        width = layout->widths[layout->column];
        if (width < 0) width = fmaxf(0, (available - fixed - ui->style.spacing*(float)(layout->columns-1))/(float)fill);
        else if (width == 0) width = cbat__ui_measure(ui, label, strlen(label)) + 2*ui->style.padding;
        rect = cbat__ui_rect(layout->x, layout->row_y, width, layout->row_height);
        layout->x += width + ui->style.spacing;
        if (++layout->column == layout->columns) { layout->column = 0; layout->y = rect.y + rect.height + ui->style.spacing; }
    }
    layout->last = rect; layout->has_last = true;
    layout->content_right = fmaxf(layout->content_right, rect.x + rect.width);
    layout->content_bottom = fmaxf(layout->content_bottom, rect.y + rect.height);
    return rect;
}
static void cbat__ui_finish_row(CBatUi *ui) {
    cbat__ui_layout *layout = &ui->layouts[ui->depth];
    if (layout->column != 0) { layout->y = layout->row_y + layout->row_height + ui->style.spacing; layout->column = 0; }
    layout->same = false;
}
static bool cbat__ui_click(CBatUi *ui, uint64_t id, CBatUiRect rect) {
    bool hover = cbat__ui_hover(ui, rect);
    if (ui->pressed && hover && ui->active == 0) ui->active = id;
    return ui->released && hover && ui->active == id;
}
static void cbat__ui_box_label(CBatUi *ui, CBatUiRect rect, const char *label, bool accent) {
    CBatUiRect text = rect;
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, rect, accent ? ui->style.accent : ui->style.panel);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT_OUTLINE, rect, ui->style.border);
    text.x += ui->style.padding; text.y += (rect.height-ui->style.font_size)/2;
    text.width = fmaxf(0, rect.width - 2*ui->style.padding); text.height = ui->style.font_size;
    cbat__ui_draw(ui, CBAT_UI_CLIP_PUSH, cbat__ui_intersect(rect, ui->layouts[ui->depth].clip), ui->style.background);
    cbat__ui_text(ui, text, label);
    cbat__ui_draw(ui, CBAT_UI_CLIP_POP, cbat__ui_rect(0,0,0,0), ui->style.background);
}

void cbat_ui_style_default(CBatUiStyle *out) {
    CBatUiStyle style = {{24,26,31,255},{45,49,58,255},{235,238,245,255},{45,115,220,255},{92,99,115,255},8,6,16,4};
    if (out != NULL) *out = style;
}
CBatUi *cbat_ui_new_with(CBatUiMeasureText measure, void *measure_context, const CBatAllocator *allocator) {
    CBatUi *ui;
    if (allocator == NULL) allocator = cbat_default_allocator();
    ui = (CBatUi *)cbat_alloc_zeroed(allocator, sizeof(*ui));
    if (ui == NULL) return NULL;
    ui->allocator = allocator; ui->measure = measure; ui->measure_context = measure_context;
    cbat_ui_style_default(&ui->style);
    ui->commands = cbat_list_new_with(sizeof(CBatUiCommand), allocator);
    ui->strings = cbat_list_new_with(sizeof(CBatString *), allocator);
    ui->fields = cbat_list_new_with(sizeof(uint64_t), allocator);
    ui->previous_fields = cbat_list_new_with(sizeof(uint64_t), allocator);
    ui->states = cbat_map_new_with(sizeof(uint64_t), sizeof(cbat__ui_state), allocator);
    if (ui->commands == NULL || ui->strings == NULL || ui->states == NULL || ui->fields == NULL || ui->previous_fields == NULL) { cbat_ui_free(ui); return NULL; }
    cbat_list_set_destructor(ui->strings, cbat_string_destructor);
    return ui;
}
CBatUi *cbat_ui_new(CBatUiMeasureText measure, void *measure_context) { return cbat_ui_new_with(measure, measure_context, NULL); }
void cbat_ui_free(CBatUi *ui) {
    if (ui == NULL) return;
    cbat_list_free(ui->commands); cbat_list_free(ui->strings); cbat_list_free(ui->fields); cbat_list_free(ui->previous_fields);
    cbat_map_free(ui->states); cbat_release(ui->allocator, ui);
}
CBatError cbat_ui_error(const CBatUi *ui) { return ui == NULL ? CBAT_ERR_INVALID_ARGUMENT : ui->error; }
size_t cbat_ui_command_count(const CBatUi *ui) { return ui == NULL ? 0 : ui->commands->length; }
const CBatUiCommand *cbat_ui_command(const CBatUi *ui, size_t index) { return ui == NULL ? NULL : (const CBatUiCommand *)cbat_list_get(ui->commands, index); }
bool cbat_ui_wants_keyboard(const CBatUi *ui) { return ui != NULL && ui->focus != 0 && ui->text_focus; }

void cbat_ui_begin_frame(CBatUi *ui, const CBatUiInput *input, float width, float height) {
    CBatUiRect viewport;
    CBatList *swap;
    if (ui == NULL) return;
    ui->error = CBAT_OK; ui->frame = true; ui->depth = 0; ui->id_depth = 0;
    ui->ids[0] = UINT64_C(14695981039346656037); ui->focus_seen = false; ui->text_focus = false;
    cbat_list_clear(ui->commands); cbat_list_clear(ui->strings);
    swap = ui->previous_fields; ui->previous_fields = ui->fields; ui->fields = swap; cbat_list_clear(ui->fields);
    if (input != NULL) ui->input = *input; else memset(&ui->input, 0, sizeof(ui->input));
    viewport = cbat__ui_rect(0, 0, width, height);
    if (!cbat__ui_valid_rect(viewport) || !isfinite(ui->input.pointer_x) || !isfinite(ui->input.pointer_y) ||
        !isfinite(ui->input.scroll_x) || !isfinite(ui->input.scroll_y) || memchr(ui->input.text, 0, sizeof(ui->input.text)) == NULL) {
        cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return;
    }
    ui->pressed = ui->input.pointer_down && !ui->previous_down; ui->released = !ui->input.pointer_down && ui->previous_down;
    if (ui->pressed) ui->focus = 0;
    if ((ui->input.keys_pressed & CBAT_UI_KEY_TAB) && ui->previous_fields->length != 0) {
        size_t i;
        for (i = 0; i < ui->previous_fields->length; i++) if (*(uint64_t *)cbat_list_get(ui->previous_fields, i) == ui->focus) break;
        i = i < ui->previous_fields->length ? (i+1)%ui->previous_fields->length : 0;
        ui->focus = *(uint64_t *)cbat_list_get(ui->previous_fields, i);
    }
    cbat__ui_layout_init(ui, &ui->layouts[0], cbat__ui_rect(ui->style.padding, ui->style.padding,
        fmaxf(0, width-2*ui->style.padding), fmaxf(0, height-2*ui->style.padding)), viewport);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, viewport, ui->style.background);
    cbat__ui_draw(ui, CBAT_UI_CLIP_PUSH, viewport, ui->style.background);
}
void cbat_ui_end_frame(CBatUi *ui) {
    if (ui == NULL) return;
    if (!ui->frame || ui->depth != 0 || ui->id_depth != 0) cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT);
    cbat__ui_draw(ui, CBAT_UI_CLIP_POP, cbat__ui_rect(0,0,0,0), ui->style.background);
    if (!ui->focus_seen) ui->focus = 0;
    if (!ui->input.pointer_down) ui->active = 0;
    ui->previous_down = ui->input.pointer_down; ui->frame = false;
    if (ui->error != CBAT_OK) cbat_list_clear(ui->commands);
}
void cbat_ui_set_style(CBatUi *ui, const CBatUiStyle *style) {
    if (ui == NULL) return;
    if (style == NULL || !isfinite(style->padding) || style->padding < 0 || !isfinite(style->spacing) || style->spacing < 0 ||
        !isfinite(style->font_size) || style->font_size <= 0 || !isfinite(style->corner_radius) || style->corner_radius < 0 ||
        !isfinite(style->font_size + 2*style->padding)) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    ui->style = *style;
}
void cbat_ui_push_id(CBatUi *ui, const char *id) {
    uint64_t hash;
    if (!cbat__ui_live(ui)) return;
    if (id == NULL || ui->id_depth == 64) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    hash = cbat__ui_id(ui, id); ui->ids[++ui->id_depth] = hash;
}
void cbat_ui_push_id_int(CBatUi *ui, int64_t id) {
    char text[21]; cbat_format_int64(id, text); cbat_ui_push_id(ui, text);
}
void cbat_ui_pop_id(CBatUi *ui) {
    if (!cbat__ui_live(ui)) return;
    if (ui->id_depth <= ui->layouts[ui->depth].id_base) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    ui->id_depth--;
}
void cbat_ui_row(CBatUi *ui, size_t columns, const float *widths, float height) {
    cbat__ui_layout *layout; size_t i;
    if (!cbat__ui_live(ui)) return;
    if (columns == 0 || columns > 64 || !isfinite(height) || height < 0) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    if (widths != NULL) for (i = 0; i < columns; i++) if (!isfinite(widths[i])) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    cbat__ui_finish_row(ui); layout = &ui->layouts[ui->depth]; layout->columns = columns;
    layout->row_height = height == 0 ? cbat__ui_height(ui) : height;
    for (i = 0; i < columns; i++) layout->widths[i] = widths == NULL ? -1 : widths[i];
}
void cbat_ui_same_line(CBatUi *ui) { if (cbat__ui_live(ui)) ui->layouts[ui->depth].same = true; }
void cbat_ui_spacing(CBatUi *ui, float pixels) {
    if (!cbat__ui_live(ui)) return;
    if (!isfinite(pixels) || pixels < 0) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    cbat__ui_finish_row(ui); ui->layouts[ui->depth].y += pixels;
}
void cbat_ui_separator(CBatUi *ui) {
    CBatUiRect rect;
    if (!cbat__ui_live(ui)) return;
    cbat__ui_finish_row(ui); rect = cbat__ui_next(ui, ""); rect.y += rect.height/2; rect.height = 0;
    cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, rect, ui->style.border);
}
void cbat_ui_label(CBatUi *ui, const char *text) {
    CBatUiRect rect;
    if (!cbat__ui_live(ui)) return;
    if (text == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    if (ui->layouts[ui->depth].hidden) return;
    rect = cbat__ui_next(ui, text); rect.y += (rect.height-ui->style.font_size)/2; rect.height = ui->style.font_size;
    cbat__ui_text(ui, rect, text);
}
bool cbat_ui_button(CBatUi *ui, const char *label) {
    CBatUiRect rect; uint64_t id; bool clicked;
    if (!cbat__ui_live(ui)) return false;
    if (label == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    if (ui->layouts[ui->depth].hidden) return false;
    id = cbat__ui_id(ui, label); rect = cbat__ui_next(ui, label); clicked = cbat__ui_click(ui, id, rect);
    cbat__ui_box_label(ui, rect, label, ui->active == id || cbat__ui_hover(ui, rect));
    return ui->error == CBAT_OK && clicked;
}
static bool cbat__ui_choice(CBatUi *ui, const char *label, bool selected, bool radio) {
    bool click; CBatUiRect rect, marker; uint64_t id;
    if (!cbat__ui_live(ui)) return false;
    if (label == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    if (ui->layouts[ui->depth].hidden) return false;
    id = cbat__ui_id(ui, label); rect = cbat__ui_next(ui, label); click = cbat__ui_click(ui, id, rect);
    marker = cbat__ui_rect(rect.x, rect.y+(rect.height-ui->style.font_size)/2, ui->style.font_size, ui->style.font_size);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, marker, (radio ? selected || click : selected != click) ? ui->style.accent : ui->style.panel);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT_OUTLINE, marker, ui->style.border);
    rect.x += ui->style.font_size + ui->style.spacing; rect.width = fmaxf(0, rect.width-ui->style.font_size-ui->style.spacing);
    cbat__ui_box_label(ui, rect, label, false);
    if (ui->error != CBAT_OK) return false;
    return click;
}
bool cbat_ui_checkbox(CBatUi *ui, const char *label, bool *value) {
    bool click;
    if (value == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    click = cbat__ui_choice(ui, label, *value, false);
    if (click) *value = !*value;
    return click;
}
bool cbat_ui_radio(CBatUi *ui, const char *label, int *selected, int value) {
    bool clicked;
    if (selected == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    clicked = cbat__ui_choice(ui, label, *selected == value, true);
    if (clicked && *selected != value) { *selected = value; return true; }
    return false;
}
bool cbat_ui_collapsing_header(CBatUi *ui, const char *label) {
    uint64_t id; cbat__ui_state *state; bool clicked;
    if (!cbat__ui_live(ui)) return false;
    if (label == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    id = cbat__ui_id(ui, label); state = cbat__ui_state_get(ui, id);
    if (state == NULL) return false;
    clicked = cbat_ui_button(ui, label);
    if (clicked) state->open = !state->open;
    if (!ui->layouts[ui->depth].hidden) {
        CBatUiRect r = ui->layouts[ui->depth].last;
        float x = r.x + r.width - ui->style.padding - 8, y = r.y + r.height/2;
        cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(x,y,8,0), ui->style.text);
        if (!state->open) cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(x+4,y-4,0,8), ui->style.text);
    }
    return state->open;
}
void cbat_ui_progress(CBatUi *ui, float fraction) {
    CBatUiRect rect;
    if (!cbat__ui_live(ui)) return;
    if (!isfinite(fraction)) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    if (ui->layouts[ui->depth].hidden) return;
    rect = cbat__ui_next(ui, ""); cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, rect, ui->style.panel);
    rect.width *= fminf(1, fmaxf(0, fraction)); cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, rect, ui->style.accent);
}

bool cbat_ui_slider_double(CBatUi *ui, const char *label, double *value, double min, double max) {
    CBatUiRect rect, fill;
    uint64_t id;
    double result, t;
    if (!cbat__ui_live(ui)) return false;
    if (label == NULL || value == NULL || !isfinite(*value) || !isfinite(min) || !isfinite(max) || min > max) {
        cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false;
    }
    if (ui->layouts[ui->depth].hidden) return false;
    id = cbat__ui_id(ui, label); rect = cbat__ui_next(ui, label);
    result = fmin(max, fmax(min, *value));
    if (ui->pressed && cbat__ui_hover(ui, rect) && ui->active == 0) { ui->active = id; ui->focus = id; }
    if (ui->active == id && ui->input.pointer_down && rect.width > 0) {
        t = fmin(1, fmax(0, (double)(ui->input.pointer_x - rect.x)/rect.width));
        result = t == 0 ? min : t == 1 ? max : (1-t)*min + t*max;
    }
    if (ui->focus == id) {
        double step = max/100 - min/100;
        ui->focus_seen = true;
        if (ui->input.keys_pressed & CBAT_UI_KEY_LEFT) result = fmax(min, result - step);
        if (ui->input.keys_pressed & CBAT_UI_KEY_RIGHT) result = fmin(max, result + step);
    }
    if (max == min) t = 0;
    else if (isfinite(max-min)) t = (result-min)/(max-min);
    else t = (result/2-min/2)/(max/2-min/2);
    fill = rect; fill.width *= (float)fmin(1, fmax(0, t));
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, rect, ui->style.panel);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, fill, ui->style.accent);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT_OUTLINE, rect, ui->style.border);
    rect.x += ui->style.padding; rect.y += (rect.height-ui->style.font_size)/2;
    rect.width = fmaxf(0, rect.width - 2*ui->style.padding); rect.height = ui->style.font_size;
    cbat__ui_text(ui, rect, label);
    if (ui->error != CBAT_OK || result == *value) return false;
    *value = result; return true;
}
bool cbat_ui_slider_int(CBatUi *ui, const char *label, int *value, int min, int max) {
    double temporary;
    uint32_t keys = ui == NULL ? 0 : ui->input.keys_pressed;
    bool focused;
    int result;
    if (value == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    focused = ui != NULL && label != NULL && ui->focus == cbat__ui_id(ui, label);
    if (ui != NULL) ui->input.keys_pressed &= ~(uint32_t)(CBAT_UI_KEY_LEFT | CBAT_UI_KEY_RIGHT);
    temporary = *value;
    (void)cbat_ui_slider_double(ui, label, &temporary, min, max);
    if (ui != NULL) ui->input.keys_pressed = keys;
    if (ui == NULL || ui->error != CBAT_OK) return false;
    result = (int)round(temporary);
    if (focused && (keys & CBAT_UI_KEY_LEFT) && result > min) result--;
    if (focused && (keys & CBAT_UI_KEY_RIGHT) && result < max) result++;
    if (result == *value) return false;
    *value = result; return true;
}

bool cbat_ui_window_begin(CBatUi *ui, const char *title, CBatUiRect *rect) {
    cbat__ui_layout *layout;
    cbat__ui_state *state;
    CBatUiRect header, body, clip;
    uint64_t id;
    float title_height;
    bool hidden;
    if (!cbat__ui_live(ui)) return false;
    if (title == NULL || rect == NULL || !cbat__ui_valid_rect(*rect) || ui->depth == 16 || ui->id_depth == 64) {
        cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false;
    }
    id = cbat__ui_id(ui, title); state = cbat__ui_state_get(ui, id);
    if (state == NULL) return false;
    title_height = fminf(rect->height, cbat__ui_height(ui));
    header = *rect; header.height = title_height;
    if (ui->pressed && cbat__ui_hover(ui, header) && ui->active == 0) {
        if (ui->input.pointer_x < header.x + title_height) state->open = !state->open;
        else { ui->active = id; ui->drag_x = ui->input.pointer_x - rect->x; ui->drag_y = ui->input.pointer_y - rect->y; }
    }
    if (ui->active == id && ui->input.pointer_down) { rect->x = ui->input.pointer_x - ui->drag_x; rect->y = ui->input.pointer_y - ui->drag_y; }
    header = *rect; header.height = title_height;
    body = *rect; body.y += title_height; body.height = fmaxf(0, body.height-title_height);
    clip = cbat__ui_intersect(body, ui->layouts[ui->depth].clip);
    hidden = ui->layouts[ui->depth].hidden || state->open;
    if (!ui->layouts[ui->depth].hidden) {
        if (!state->open) cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, *rect, ui->style.background);
        CBatUiRect title_rect = header;
        float x = header.x+title_height/2, y = header.y+title_height/2;
        title_rect.x += title_height; title_rect.width = fmaxf(0, title_rect.width-title_height);
        cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, header, ui->style.panel);
        cbat__ui_box_label(ui, title_rect, title, ui->active == id);
        cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(x-4,y,8,0), ui->style.text);
        if (state->open) cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(x,y-4,0,8), ui->style.text);
        cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(header.x+title_height-4, header.y+4, 0, fmaxf(0,title_height-8)), ui->style.border);
    }
    if (!hidden && cbat__ui_inside(ui, clip)) {
        state->scroll_x = fminf(state->max_x, fmaxf(0, state->scroll_x + ui->input.scroll_x));
        state->scroll_y = fminf(state->max_y, fmaxf(0, state->scroll_y + ui->input.scroll_y));
    }
    ui->ids[++ui->id_depth] = id;
    layout = &ui->layouts[++ui->depth];
    cbat__ui_layout_init(ui, layout, cbat__ui_rect(body.x+ui->style.padding-state->scroll_x, body.y+ui->style.padding-state->scroll_y,
        fmaxf(0, body.width-2*ui->style.padding), fmaxf(0, body.height-2*ui->style.padding)), clip);
    layout->window = id; layout->id_base = ui->id_depth; layout->hidden = hidden;
    cbat__ui_draw(ui, CBAT_UI_CLIP_PUSH, hidden ? cbat__ui_rect(clip.x,clip.y,0,0) : clip, ui->style.background);
    return ui->error == CBAT_OK && !hidden;
}
void cbat_ui_window_end(CBatUi *ui) {
    cbat__ui_layout *layout; cbat__ui_state *state;
    if (!cbat__ui_live(ui)) return;
    if (ui->depth == 0) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    layout = &ui->layouts[ui->depth];
    if (ui->id_depth != layout->id_base) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return; }
    state = (cbat__ui_state *)cbat_map_get(ui->states, &layout->window);
    if (state != NULL && !layout->hidden) {
        state->max_x = fmaxf(0, layout->content_right - layout->bounds.x - layout->bounds.width);
        state->max_y = fmaxf(0, layout->content_bottom - layout->bounds.y - layout->bounds.height);
        state->scroll_x = fminf(state->scroll_x, state->max_x); state->scroll_y = fminf(state->scroll_y, state->max_y);
    }
    cbat__ui_draw(ui, CBAT_UI_CLIP_POP, cbat__ui_rect(0,0,0,0), ui->style.background);
    ui->depth--; ui->id_depth--;
}

static size_t cbat__ui_previous(const char *text, size_t at) {
    if (at == 0) return 0;
    at--; while (at != 0 && ((unsigned char)text[at] & 0xc0) == 0x80) at--;
    return at;
}
static size_t cbat__ui_following(const char *text, size_t length, size_t at) {
    if (at == length) return at;
    at++; while (at < length && ((unsigned char)text[at] & 0xc0) == 0x80) at++;
    return at;
}
bool cbat_ui_text_input(CBatUi *ui, const char *label, CBatString *text) {
    uint64_t id;
    cbat__ui_state *state;
    CBatUiRect rect, content;
    CBatString *edited = NULL;
    const CBatString *shown = text;
    size_t cursor, typed_length, i;
    bool changed = false, focused;
    CBatError err = CBAT_OK;
    if (!cbat__ui_live(ui)) return false;
    if (label == NULL || text == NULL) { cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false; }
    if (ui->layouts[ui->depth].hidden) return false;
    if (!cbat_utf8_is_valid(text->data, text->length)) { cbat__ui_fail(ui, CBAT_ERR_INVALID_UTF8); return false; }
    if (memchr(text->data, 0, text->length) != NULL || memchr(text->data, '\n', text->length) != NULL || memchr(text->data, '\r', text->length) != NULL) {
        cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false;
    }
    typed_length = strlen(ui->input.text);
    if (!cbat_utf8_is_valid(ui->input.text, typed_length)) { cbat__ui_fail(ui, CBAT_ERR_INVALID_UTF8); return false; }
    for (i = 0; i < typed_length; i++) if ((unsigned char)ui->input.text[i] < 32 || ui->input.text[i] == 127) {
        cbat__ui_fail(ui, CBAT_ERR_INVALID_ARGUMENT); return false;
    }
    id = cbat__ui_id(ui, label); state = cbat__ui_state_get(ui, id);
    if (state == NULL) return false;
    err = cbat_list_push(ui->fields, &id);
    if (err != CBAT_OK) { cbat__ui_fail(ui, err); return false; }
    cursor = state->cursor > text->length ? text->length : state->cursor;
    while (cursor != 0 && cursor < text->length && ((unsigned char)text->data[cursor] & 0xc0) == 0x80) cursor--;
    rect = cbat__ui_next(ui, label);
    content = cbat__ui_rect(rect.x+ui->style.padding, rect.y+(rect.height-ui->style.font_size)/2,
        fmaxf(0, rect.width-2*ui->style.padding), ui->style.font_size);
    if (ui->pressed && cbat__ui_hover(ui, rect)) {
        float x = ui->input.pointer_x - content.x;
        cursor = 0; ui->focus = id;
        while (cursor < text->length) {
            size_t next = cbat__ui_following(text->data, text->length, cursor);
            float left = cbat__ui_measure(ui, text->data, cursor), right = cbat__ui_measure(ui, text->data, next);
            if (x < (left+right)/2) break;
            cursor = next;
        }
    }
    focused = ui->focus == id;
    if (focused) {
        uint32_t keys = ui->input.keys_pressed;
        ui->focus_seen = true; ui->text_focus = true;
        if (keys & (CBAT_UI_KEY_ENTER | CBAT_UI_KEY_ESCAPE)) { ui->focus = 0; focused = false; ui->text_focus = false; }
        else {
            if (keys & CBAT_UI_KEY_HOME) cursor = 0;
            if (keys & CBAT_UI_KEY_END) cursor = text->length;
            if (keys & CBAT_UI_KEY_LEFT) cursor = cbat__ui_previous(text->data, cursor);
            if (keys & CBAT_UI_KEY_RIGHT) cursor = cbat__ui_following(text->data, text->length, cursor);
            if (typed_length != 0 || (keys & (CBAT_UI_KEY_BACKSPACE | CBAT_UI_KEY_DELETE))) {
                edited = cbat_string_new_with(text->allocator);
                if (edited == NULL) err = CBAT_ERR_NO_MEMORY;
                if (err == CBAT_OK) err = cbat_string_append_length(edited, text->data, text->length);
                if (err == CBAT_OK && (keys & CBAT_UI_KEY_BACKSPACE) && cursor != 0) {
                    size_t previous = cbat__ui_previous(edited->data, cursor);
                    err = cbat_string_erase(edited, previous, cursor-previous); cursor = previous;
                }
                if (err == CBAT_OK && (keys & CBAT_UI_KEY_DELETE) && cursor < edited->length)
                    err = cbat_string_erase(edited, cursor, cbat__ui_following(edited->data, edited->length, cursor)-cursor);
                if (err == CBAT_OK && typed_length != 0) { err = cbat_string_insert(edited, cursor, ui->input.text); cursor += typed_length; }
                if (err == CBAT_OK) { shown = edited; changed = edited->length != text->length || memcmp(edited->data, text->data, text->length) != 0; }
            }
        }
    }
    cbat__ui_fail(ui, err);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT, rect, ui->style.panel);
    cbat__ui_draw(ui, CBAT_UI_DRAW_RECT_OUTLINE, rect, focused ? ui->style.accent : ui->style.border);
    cbat__ui_draw(ui, CBAT_UI_CLIP_PUSH, cbat__ui_intersect(rect, ui->layouts[ui->depth].clip), ui->style.background);
    if (focused && ui->error == CBAT_OK) {
        float caret = cbat__ui_measure(ui, shown->data, cursor);
        if (caret > content.width) content.x -= caret-content.width;
        cbat__ui_text(ui, content, shown->data);
        cbat__ui_draw(ui, CBAT_UI_DRAW_LINE, cbat__ui_rect(content.x+caret, content.y, 0, content.height), ui->style.accent);
    } else cbat__ui_text(ui, content, shown->data);
    cbat__ui_draw(ui, CBAT_UI_CLIP_POP, cbat__ui_rect(0,0,0,0), ui->style.background);
    if (ui->error == CBAT_OK) {
        state->cursor = cursor;
        if (changed) { CBatString old = *text; *text = *edited; *edited = old; }
    } else changed = false;
    cbat_string_free(edited);
    return changed;
}
