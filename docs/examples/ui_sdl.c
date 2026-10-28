/* SDL2 + SDL2_ttf reference host for cbat/ui.h. MIT License.
 * Build via docs/examples/CMakeLists.txt, CBAT_BUILD_SDL_EXAMPLE=ON.
 * Usage: ui_sdl path/to/font.ttf [--smoke [screenshot.bmp]]
 * The host owns fonts, event translation, clipping and rendering. The UI
 * library supplies only input-independent state and a flat command list.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cbat/ui.h"

typedef struct Host { TTF_Font *font; CBatString *scratch; } Host;
static float measure(void *context, const char *text, size_t length, float size) {
    Host *host = (Host *)context;
    int width = 0, height = 0;
    cbat_string_clear(host->scratch);
    if (cbat_string_append_length(host->scratch, text, length) != CBAT_OK ||
        TTF_SizeUTF8(host->font, host->scratch->data, &width, &height) != 0) return NAN;
    return (float)width * size / (float)TTF_FontHeight(host->font);
}
static int pixel(float value) { return (int)fmaxf(-1000000, fminf(1000000, value)); }
static SDL_Rect sdl_rect(CBatUiRect rect) {
    SDL_Rect result = {pixel(rect.x), pixel(rect.y), pixel(rect.width), pixel(rect.height)};
    return result;
}
static int rounded(SDL_Renderer *renderer, SDL_Rect rect, float radius, bool outline) {
    int y, r = pixel(fminf(radius, (float)(rect.w < rect.h ? rect.w : rect.h)/2));
    if (rect.w <= 0 || rect.h <= 0) return 0;
    if (r <= 0) return outline ? SDL_RenderDrawRect(renderer, &rect) : SDL_RenderFillRect(renderer, &rect);
    for (y = 0; y < rect.h; y++) {
        int inset = 0;
        if (y < r || y >= rect.h-r) {
            float dy = y < r ? (float)(r-y)-0.5f : (float)(y-(rect.h-r))+0.5f;
            inset = r - (int)sqrtf(fmaxf(0, (float)r*(float)r-dy*dy));
        }
        if (outline && y != 0 && y != rect.h-1) {
            if (SDL_RenderDrawPoint(renderer, rect.x+inset, rect.y+y) != 0 ||
                SDL_RenderDrawPoint(renderer, rect.x+rect.w-1-inset, rect.y+y) != 0) return -1;
        } else if (SDL_RenderDrawLine(renderer, rect.x+inset, rect.y+y, rect.x+rect.w-1-inset, rect.y+y) != 0) return -1;
    }
    return 0;
}
static bool render(SDL_Renderer *renderer, Host *host, const CBatUi *ui) {
    SDL_Rect clips[64]; size_t depth = 0, i;
    if (SDL_RenderSetClipRect(renderer, NULL) != 0 || SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) != 0) return false;
    for (i = 0; i < cbat_ui_command_count(ui); i++) {
        const CBatUiCommand *command = cbat_ui_command(ui, i);
        SDL_Rect rect = sdl_rect(command->rect);
        int status = 0;
        if (command->type == CBAT_UI_CLIP_PUSH) {
            SDL_Rect intersection = {0,0,0,0};
            if (depth == 64) return false;
            if (depth != 0) { (void)SDL_IntersectRect(&clips[depth-1], &rect, &intersection); rect = intersection; }
            clips[depth++] = rect;
            if (SDL_RenderSetClipRect(renderer, &rect) != 0) return false;
            continue;
        }
        if (command->type == CBAT_UI_CLIP_POP) {
            if (depth == 0) return false;
            depth--; if (SDL_RenderSetClipRect(renderer, depth == 0 ? NULL : &clips[depth-1]) != 0) return false;
            continue;
        }
        if (SDL_SetRenderDrawColor(renderer, command->color.r, command->color.g, command->color.b, command->color.a) != 0) return false;
        if (command->type == CBAT_UI_DRAW_RECT || command->type == CBAT_UI_DRAW_RECT_OUTLINE)
            status = rounded(renderer, rect, command->corner_radius, command->type == CBAT_UI_DRAW_RECT_OUTLINE);
        else if (command->type == CBAT_UI_DRAW_LINE)
            status = SDL_RenderDrawLine(renderer, rect.x, rect.y, pixel(command->rect.x+command->rect.width), pixel(command->rect.y+command->rect.height));
        else if (command->type == CBAT_UI_DRAW_TEXT && command->text_length != 0) {
            SDL_Color color = {command->color.r, command->color.g, command->color.b, command->color.a};
            SDL_Surface *surface = TTF_RenderUTF8_Blended(host->font, command->text, color);
            SDL_Texture *texture;
            float scale = command->font_size/(float)TTF_FontHeight(host->font);
            if (surface == NULL) return false;
            rect.w = pixel((float)surface->w*scale); rect.h = pixel((float)surface->h*scale);
            texture = SDL_CreateTextureFromSurface(renderer, surface); SDL_FreeSurface(surface);
            if (texture == NULL) return false;
            status = SDL_RenderCopy(renderer, texture, NULL, &rect); SDL_DestroyTexture(texture);
        }
        if (status != 0) return false;
    }
    return depth == 0;
}
static uint32_t key(SDL_Keycode code) {
    switch (code) {
    case SDLK_BACKSPACE: return CBAT_UI_KEY_BACKSPACE;
    case SDLK_DELETE: return CBAT_UI_KEY_DELETE;
    case SDLK_LEFT: return CBAT_UI_KEY_LEFT;
    case SDLK_RIGHT: return CBAT_UI_KEY_RIGHT;
    case SDLK_RETURN: return CBAT_UI_KEY_ENTER;
    case SDLK_TAB: return CBAT_UI_KEY_TAB;
    case SDLK_HOME: return CBAT_UI_KEY_HOME;
    case SDLK_END: return CBAT_UI_KEY_END;
    case SDLK_ESCAPE: return CBAT_UI_KEY_ESCAPE;
    default: return 0;
    }
}
int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    Host host = {NULL,NULL};
    CBatUi *ui = NULL;
    CBatString *name = NULL;
    CBatUiRect panel = {30,25,460,520};
    CBatUiStyle style;
    bool quit = false, enabled = true, smoke = argc > 2 && strcmp(argv[2], "--smoke") == 0;
    int selected = 0, clicks = 0, result = 1;
    double volume = 0.65;
    if (argc < 2) { fprintf(stderr, "Usage: ui_sdl font.ttf [--smoke [screenshot.bmp]]\n"); return 1; }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) goto cleanup;
    host.font = TTF_OpenFont(argv[1], 16); host.scratch = cbat_string_new(); name = cbat_string_from("Ada Lovelace");
    if (host.font == NULL || host.scratch == NULL || name == NULL) goto cleanup;
    window = SDL_CreateWindow("cbat immediate-mode UI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
        smoke ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE);
    if (window == NULL) goto cleanup;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    ui = cbat_ui_new(measure, &host);
    if (renderer == NULL || ui == NULL) goto cleanup;
    cbat_ui_style_default(&style); style.font_size = (float)TTF_FontHeight(host.font); cbat_ui_set_style(ui, &style);
    SDL_StartTextInput();
    while (!quit) {
        SDL_Event event;
        CBatUiInput input = {0};
        int width, height, mouse_x, mouse_y;
        char status[80];
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            else if (event.type == SDL_MOUSEWHEEL) {
                float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1.0f : -1.0f;
                input.scroll_x += direction*(float)event.wheel.x*30; input.scroll_y += direction*(float)event.wheel.y*30;
            } else if (event.type == SDL_KEYDOWN) input.keys_pressed |= key(event.key.keysym.sym);
            else if (event.type == SDL_TEXTINPUT) {
                size_t used = strlen(input.text), count = strlen(event.text.text);
                if (count < sizeof(input.text)-used) memcpy(input.text+used, event.text.text, count+1);
            }
        }
        input.pointer_down = (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON_LMASK) != 0;
        input.pointer_x = (float)mouse_x; input.pointer_y = (float)mouse_y;
        SDL_GetWindowSize(window, &width, &height);
        cbat_ui_begin_frame(ui, &input, (float)width, (float)height);
        if (cbat_ui_window_begin(ui, "Settings (drag title; +/- collapses)", &panel)) {
            cbat_ui_label(ui, "Renderer-independent C widgets");
            cbat_ui_label(ui, "Name"); (void)cbat_ui_text_input(ui, "name", name);
            (void)cbat_ui_checkbox(ui, "Enable notifications", &enabled);
            (void)cbat_ui_slider_double(ui, "Volume (drag or arrow keys)", &volume, 0, 1);
            cbat_ui_progress(ui, (float)volume);
            cbat_ui_row(ui, 2, NULL, 0);
            (void)cbat_ui_radio(ui, "Light", &selected, 0); (void)cbat_ui_radio(ui, "Dark", &selected, 1);
            cbat_ui_row(ui, 1, NULL, 0);
            if (cbat_ui_button(ui, "Apply settings")) clicks++;
            snprintf(status, sizeof(status), "Applied %d times; volume %.0f%%", clicks, volume*100); cbat_ui_label(ui, status);
            if (cbat_ui_collapsing_header(ui, "Details")) cbat_ui_label(ui, "Text, rectangles, lines and nested clips.");
        }
        cbat_ui_window_end(ui); cbat_ui_end_frame(ui);
        if (cbat_ui_error(ui) != CBAT_OK) { fprintf(stderr, "%s\n", cbat_error_str(cbat_ui_error(ui))); goto cleanup; }
        if (!render(renderer, &host, ui)) goto cleanup;
        if (smoke && argc > 3) {
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
            bool saved;
            if (surface == NULL) goto cleanup;
            saved = SDL_RenderReadPixels(renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0 && SDL_SaveBMP(surface, argv[3]) == 0;
            SDL_FreeSurface(surface); if (!saved) goto cleanup;
        }
        SDL_RenderPresent(renderer);
        if (smoke) quit = true; else SDL_Delay(16);
    }
    result = 0;
cleanup:
    if (result != 0) fprintf(stderr, "SDL host: %s / %s\n", SDL_GetError(), TTF_GetError());
    cbat_ui_free(ui); cbat_string_free(name); cbat_string_free(host.scratch);
    if (host.font != NULL) TTF_CloseFont(host.font);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit(); SDL_Quit(); return result;
}
