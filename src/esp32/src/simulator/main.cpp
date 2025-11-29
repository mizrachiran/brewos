/**
 * BrewOS UI Theme Simulator
 * 
 * Standalone LVGL simulator for testing theme colors and UI components
 * 
 * Build: pio run -e simulator
 * Run:   .pio/build/simulator/program
 */

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <unistd.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// =============================================================================
// Configuration
// =============================================================================

#define DISP_HOR_RES 480
#define DISP_VER_RES 480

// =============================================================================
// BrewOS Brand Colors (from theme.h)
// =============================================================================

#define COLOR_BG_DARK           lv_color_hex(0x1A0F0A)  // Darkest coffee
#define COLOR_BG_CARD           lv_color_hex(0x361E12)  // Dark brown (brand primary)
#define COLOR_BG_ELEVATED       lv_color_hex(0x4A2A1A)  // Elevated surface

#define COLOR_ACCENT_PRIMARY    lv_color_hex(0xD5A071)  // Caramel/tan (brand accent)
#define COLOR_ACCENT_ORANGE     lv_color_hex(0xC4703C)  // Warm orange
#define COLOR_ACCENT_COPPER     lv_color_hex(0x714C30)  // Medium brown

#define COLOR_TEXT_PRIMARY      lv_color_hex(0xFBFCF8)  // Cream white
#define COLOR_TEXT_SECONDARY    lv_color_hex(0xD5A071)  // Caramel
#define COLOR_TEXT_MUTED        lv_color_hex(0x9B6E46)  // Light coffee

#define COLOR_TEMP_COLD         lv_color_hex(0x3B82F6)  // Blue
#define COLOR_TEMP_WARM         lv_color_hex(0xF59E0B)  // Yellow
#define COLOR_TEMP_READY        lv_color_hex(0x22C55E)  // Green
#define COLOR_TEMP_HOT          lv_color_hex(0xEF4444)  // Red

#define COLOR_SUCCESS           lv_color_hex(0x22C55E)
#define COLOR_WARNING           lv_color_hex(0xF59E0B)
#define COLOR_ERROR             lv_color_hex(0xDC2626)

// =============================================================================
// SDL Variables
// =============================================================================

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static lv_color_t* fb = NULL;

// =============================================================================
// Display Driver
// =============================================================================

static void sdl_display_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            fb[y * DISP_HOR_RES + x] = *color_p;
            color_p++;
        }
    }
    
    SDL_UpdateTexture(texture, NULL, fb, DISP_HOR_RES * sizeof(lv_color_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    lv_disp_flush_ready(drv);
}

// =============================================================================
// Input Driver
// =============================================================================

static bool mouse_pressed = false;
static int16_t mouse_x = 0;
static int16_t mouse_y = 0;

static void sdl_mouse_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static bool handle_sdl_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_MOUSEMOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) mouse_pressed = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) mouse_pressed = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) return false;
                break;
        }
    }
    return true;
}

// =============================================================================
// Time
// =============================================================================

static uint32_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}


// =============================================================================
// Demo UI - Home Screen Mockup
// =============================================================================

static lv_obj_t* temp_label = NULL;
static lv_obj_t* temp_arc = NULL;
static lv_obj_t* steam_label = NULL;
static lv_obj_t* pressure_bar = NULL;
static lv_obj_t* status_label = NULL;

static float mock_brew_temp = 93.5f;
static float mock_steam_temp = 145.0f;
static float mock_pressure = 9.0f;

static void create_demo_ui(void) {
    // Dark background
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG_DARK, 0);
    
    // === Title ===
    lv_obj_t* title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "BrewOS Theme Preview");
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    // === Brew Temperature Arc (center) ===
    temp_arc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(temp_arc, 200, 200);
    lv_obj_align(temp_arc, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_range(temp_arc, 0, 100);
    lv_arc_set_value(temp_arc, 94);
    lv_arc_set_bg_angles(temp_arc, 135, 45);
    
    // Arc styling
    lv_obj_set_style_arc_color(temp_arc, COLOR_BG_ELEVATED, LV_PART_MAIN);
    lv_obj_set_style_arc_width(temp_arc, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_color(temp_arc, COLOR_TEMP_READY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(temp_arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(temp_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(temp_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    
    // Temperature label inside arc
    temp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(temp_label, "93.5°");
    lv_obj_set_style_text_color(temp_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_48, 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -30);
    
    lv_obj_t* brew_label = lv_label_create(lv_scr_act());
    lv_label_set_text(brew_label, "BREW");
    lv_obj_set_style_text_color(brew_label, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(brew_label, &lv_font_montserrat_14, 0);
    lv_obj_align(brew_label, LV_ALIGN_CENTER, 0, 10);
    
    // === Steam Temperature (bottom left) ===
    lv_obj_t* steam_card = lv_obj_create(lv_scr_act());
    lv_obj_set_size(steam_card, 120, 70);
    lv_obj_align(steam_card, LV_ALIGN_BOTTOM_LEFT, 40, -60);
    lv_obj_set_style_bg_color(steam_card, COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(steam_card, 12, 0);
    lv_obj_set_style_border_width(steam_card, 0, 0);
    lv_obj_set_style_pad_all(steam_card, 10, 0);
    
    lv_obj_t* steam_title = lv_label_create(steam_card);
    lv_label_set_text(steam_title, "Steam");
    lv_obj_set_style_text_color(steam_title, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(steam_title, &lv_font_montserrat_12, 0);
    lv_obj_align(steam_title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    steam_label = lv_label_create(steam_card);
    lv_label_set_text(steam_label, "145°");
    lv_obj_set_style_text_color(steam_label, COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(steam_label, &lv_font_montserrat_28, 0);
    lv_obj_align(steam_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // === Pressure Bar (bottom right) ===
    lv_obj_t* press_card = lv_obj_create(lv_scr_act());
    lv_obj_set_size(press_card, 120, 70);
    lv_obj_align(press_card, LV_ALIGN_BOTTOM_RIGHT, -40, -60);
    lv_obj_set_style_bg_color(press_card, COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(press_card, 12, 0);
    lv_obj_set_style_border_width(press_card, 0, 0);
    lv_obj_set_style_pad_all(press_card, 10, 0);
    
    lv_obj_t* press_title = lv_label_create(press_card);
    lv_label_set_text(press_title, "Pressure");
    lv_obj_set_style_text_color(press_title, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(press_title, &lv_font_montserrat_12, 0);
    lv_obj_align(press_title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t* press_val = lv_label_create(press_card);
    lv_label_set_text(press_val, "9.0 bar");
    lv_obj_set_style_text_color(press_val, COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(press_val, &lv_font_montserrat_20, 0);
    lv_obj_align(press_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // === Status at top ===
    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "● READY");
    lv_obj_set_style_text_color(status_label, COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // === Color Palette Preview (bottom) ===
    int colors[] = {0x361E12, 0x714C30, 0x9B6E46, 0xD5A071, 0xC4703C, 0xFBFCF8};
    int num_colors = 6;
    int box_size = 30;
    int start_x = (DISP_HOR_RES - (num_colors * (box_size + 5))) / 2;
    
    for (int i = 0; i < num_colors; i++) {
        lv_obj_t* box = lv_obj_create(lv_scr_act());
        lv_obj_set_size(box, box_size, box_size);
        lv_obj_set_pos(box, start_x + i * (box_size + 5), DISP_VER_RES - 50);
        lv_obj_set_style_bg_color(box, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_radius(box, 6, 0);
        lv_obj_set_style_border_width(box, 0, 0);
    }
    
    // === Primary Button ===
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 140, 45);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 100);
    lv_obj_set_style_bg_color(btn, COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 20, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "START BREW");
    lv_obj_set_style_text_color(btn_label, COLOR_BG_DARK, 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_label);
}

static void update_demo_ui(void) {
    static uint32_t last_update = 0;
    uint32_t now = get_time_ms();
    
    if (now - last_update > 500) {
        // Simulate temperature fluctuations
        mock_brew_temp += ((rand() % 10) - 5) * 0.05f;
        if (mock_brew_temp < 90) mock_brew_temp = 90;
        if (mock_brew_temp > 96) mock_brew_temp = 96;
        
        // Update temperature label
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°", mock_brew_temp);
        lv_label_set_text(temp_label, buf);
        
        // Update arc value (scaled 0-100)
        int arc_val = (int)((mock_brew_temp - 80) * 5);  // 80-100°C -> 0-100
        lv_arc_set_value(temp_arc, arc_val);
        
        // Update arc color based on temp vs setpoint (94°C)
        float diff = mock_brew_temp - 94.0f;
        lv_color_t arc_color;
        if (diff < -2) arc_color = COLOR_TEMP_WARM;
        else if (diff > 2) arc_color = COLOR_TEMP_HOT;
        else arc_color = COLOR_TEMP_READY;
        lv_obj_set_style_arc_color(temp_arc, arc_color, LV_PART_INDICATOR);
        
        last_update = now;
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║    BrewOS UI Theme Simulator         ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║  Press ESC to exit                   ║\n");
    printf("║  Click to simulate touch             ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n");
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    window = SDL_CreateWindow(
        "BrewOS Theme Preview (480x480)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISP_HOR_RES, DISP_VER_RES,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING,
                                DISP_HOR_RES, DISP_VER_RES);
    
    fb = (lv_color_t*)malloc(DISP_HOR_RES * DISP_VER_RES * sizeof(lv_color_t));
    memset(fb, 0, DISP_HOR_RES * DISP_VER_RES * sizeof(lv_color_t));
    
    // Initialize LVGL
    lv_init();
    
    // Display driver
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[DISP_HOR_RES * 40];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_HOR_RES * 40);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
    
    // Create demo UI
    create_demo_ui();
    
    printf("Simulator running!\n");
    
    // Main loop
    while (handle_sdl_events()) {
        update_demo_ui();
        lv_timer_handler();
        usleep(5000);
    }
    
    // Cleanup
    free(fb);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
