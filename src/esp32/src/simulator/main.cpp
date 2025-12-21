/**
 * BrewOS UI Simulator
 * 
 * Runs the actual LVGL UI screens on desktop for development
 * 
 * Build: pio run -e simulator
 * Run:   .pio/build/simulator/program
 */

#include <SDL2/SDL.h>
#include <lvgl.h>
#include "platform/platform.h"
#include "display/theme.h"
#include "display/display_config.h"
#include "ui/ui.h"
#include "ui/screen_idle.h"
#include "ui/screen_settings.h"
#include "ui/screen_cloud.h"

// =============================================================================
// Configuration
// =============================================================================

#define WINDOW_TITLE "BrewOS UI Simulator (480x480)"

// =============================================================================
// SDL Display Driver
// =============================================================================

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static lv_color_t* fb = NULL;

static void sdl_display_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            fb[y * DISPLAY_WIDTH + x] = *color_p;
            color_p++;
        }
    }
    
    SDL_UpdateTexture(texture, NULL, fb, DISPLAY_WIDTH * sizeof(lv_color_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    lv_disp_flush_ready(drv);
}

// =============================================================================
// Encoder Input Driver
// =============================================================================

static int16_t encoder_diff = 0;
static bool encoder_pressed = false;
static uint32_t press_start_time = 0;
static bool long_press_sent = false;

#define LONG_PRESS_TIME_MS 1000

static void encoder_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    data->enc_diff = encoder_diff;
    encoder_diff = 0;
    data->state = encoder_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static bool handle_sdl_events(void) {
    SDL_Event event;
    uint32_t now = platform_millis();
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
                
            case SDL_MOUSEWHEEL:
                {
                    int dir = (event.wheel.y > 0) ? -1 : 1;
                    encoder_diff += dir;
                    LOG_I("🔄 Rotate %s", dir > 0 ? "CW" : "CCW");
                    
                    // Direct call to screen-specific rotation handlers
                    screen_id_t current = ui.getCurrentScreen();
                    if (current == SCREEN_IDLE) {
                        screen_idle_select_power_mode((int)screen_idle_get_selected_power_mode() + dir);
                    } else if (current == SCREEN_SETTINGS) {
                        screen_settings_navigate(dir);
                    }
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    encoder_pressed = true;
                    press_start_time = now;
                    long_press_sent = false;
                    LOG_I("🔘 Button DOWN");
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (!long_press_sent) {
                        LOG_I("👆 Click");
                    }
                    encoder_pressed = false;
                }
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return false;
                    case SDLK_UP:
                    case SDLK_LEFT:
                        {
                            encoder_diff -= 1;
                            LOG_I("🔄 Rotate CCW");
                            screen_id_t current = ui.getCurrentScreen();
                            if (current == SCREEN_IDLE) {
                                screen_idle_select_power_mode((int)screen_idle_get_selected_power_mode() - 1);
                            } else if (current == SCREEN_SETTINGS) {
                                screen_settings_navigate(-1);
                            }
                        }
                        break;
                    case SDLK_DOWN:
                    case SDLK_RIGHT:
                        {
                            encoder_diff += 1;
                            LOG_I("🔄 Rotate CW");
                            screen_id_t current = ui.getCurrentScreen();
                            if (current == SCREEN_IDLE) {
                                screen_idle_select_power_mode((int)screen_idle_get_selected_power_mode() + 1);
                            } else if (current == SCREEN_SETTINGS) {
                                screen_settings_navigate(1);
                            }
                        }
                        break;
                    case SDLK_RETURN:
                    case SDLK_SPACE:
                        if (!encoder_pressed) {
                            encoder_pressed = true;
                            press_start_time = now;
                            long_press_sent = false;
                            LOG_I("🔘 Button DOWN");
                        }
                        break;
                    // Number keys to switch screens
                    case SDLK_0: 
                        LOG_I("📺 Screen: Setup");
                        ui.showScreen(SCREEN_SETUP); 
                        break;
                    case SDLK_1: 
                        LOG_I("📺 Screen: Idle");
                        ui.showScreen(SCREEN_IDLE); 
                        lv_group_focus_obj(lv_group_get_focused(lv_group_get_default()));
                        break;
                    case SDLK_2: 
                        LOG_I("📺 Screen: Home");
                        ui.showScreen(SCREEN_HOME); 
                        lv_group_focus_obj(lv_group_get_focused(lv_group_get_default()));
                        break;
                    case SDLK_3: 
                        LOG_I("📺 Screen: Brewing");
                        ui.showScreen(SCREEN_BREWING); 
                        break;
                    case SDLK_4: 
                        LOG_I("📺 Screen: Complete");
                        ui.showScreen(SCREEN_COMPLETE); 
                        break;
                    case SDLK_5: 
                        LOG_I("📺 Screen: Settings");
                        ui.showScreen(SCREEN_SETTINGS); 
                        lv_group_focus_obj(lv_group_get_focused(lv_group_get_default()));
                        break;
                    case SDLK_6: 
                        LOG_I("📺 Screen: Temperature");
                        ui.showScreen(SCREEN_TEMP_SETTINGS); 
                        break;
                    case SDLK_7: 
                        LOG_I("📺 Screen: Scale");
                        ui.showScreen(SCREEN_SCALE); 
                        break;
                    case SDLK_8: 
                        LOG_I("📺 Screen: Cloud");
                        ui.showScreen(SCREEN_CLOUD); 
                        break;
                    case SDLK_9: 
                        LOG_I("📺 Screen: Alarm");
                        ui.showScreen(SCREEN_ALARM); 
                        break;
                    case SDLK_o: 
                        LOG_I("📺 Screen: OTA");
                        ui.showScreen(SCREEN_OTA); 
                        break;
                    case SDLK_p: 
                        LOG_I("📺 Screen: Splash");
                        ui.showScreen(SCREEN_SPLASH); 
                        break;
                    
                }
                break;
                
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                    if (!long_press_sent) {
                        LOG_I("👆 Click");
                    }
                    encoder_pressed = false;
                }
                break;
        }
    }
    
    // Long press detection
    if (encoder_pressed && !long_press_sent && (now - press_start_time >= LONG_PRESS_TIME_MS)) {
        LOG_I("👇 LONG PRESS!");
        long_press_sent = true;
        ui.handleLongPress();
    }
    
    return true;
}

// =============================================================================
// Mock Machine State
// =============================================================================

static ui_state_t mock_state = {
    .brew_temp = 93.5f,
    .brew_setpoint = 94.0f,
    .steam_temp = 145.0f,
    .steam_setpoint = 145.0f,
    .brew_max_temp = 105.0f,
    .steam_max_temp = 160.0f,
    .pressure = 9.0f,
    .machine_state = UI_STATE_READY,
    .heating_strategy = HEAT_SEQUENTIAL,
    .is_brewing = false,
    .is_heating = false,
    .water_low = false,
    .alarm_active = false,
    .alarm_code = 0,
    .brew_time_ms = 0,
    .brew_weight = 0.0f,
    .target_weight = 36.0f,
    .dose_weight = 18.0f,
    .flow_rate = 0.0f,
    .pico_connected = true,
    .wifi_connected = true,
    .mqtt_connected = true,
    .scale_connected = false,
    .wifi_ssid = "HomeWiFi",
    .wifi_password = "",
    .wifi_ip = "192.168.1.100",
    .wifi_rssi = -45,
    .wifi_ap_mode = false
};

static void update_mock_state(void) {
    static uint32_t last_update = 0;
    uint32_t now = platform_millis();
    
    if (now - last_update > 500) {
        // Simulate temperature fluctuations
        mock_state.brew_temp += ((rand() % 10) - 5) * 0.05f;
        if (mock_state.brew_temp < 90) mock_state.brew_temp = 90;
        if (mock_state.brew_temp > 96) mock_state.brew_temp = 96;
        
        mock_state.steam_temp += ((rand() % 10) - 5) * 0.1f;
        if (mock_state.steam_temp < 140) mock_state.steam_temp = 140;
        if (mock_state.steam_temp > 150) mock_state.steam_temp = 150;
        
        // Update UI with new state
        ui.update(mock_state);
        
        last_update = now;
    }
}

// =============================================================================
// Round Display Mask
// =============================================================================

static void create_round_mask(void) {
    // Create corner masks to simulate round display
    // The actual display is circular, so corners should be black
    
    lv_obj_t* mask = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, DISPLAY_WIDTH + 4, DISPLAY_HEIGHT + 4);
    lv_obj_center(mask);
    lv_obj_set_style_radius(mask, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(mask, 60, 0);  // Black border for corners
    lv_obj_set_style_border_color(mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    
    // Inner circle outline (display boundary)
    lv_obj_t* outline = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(outline);
    lv_obj_set_size(outline, DISPLAY_WIDTH - 2, DISPLAY_HEIGHT - 2);
    lv_obj_center(outline);
    lv_obj_set_style_radius(outline, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_border_color(outline, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_CLICKABLE);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║           BrewOS UI Simulator                        ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  ENCODER:                                            ║\n");
    printf("║    Scroll / ↑↓        Rotate knob                    ║\n");
    printf("║    Click / Enter      Press button                   ║\n");
    printf("║    Hold 1 sec         Long press (go back)           ║\n");
    printf("║                                                      ║\n");
    printf("║  SCREENS (number keys):                              ║\n");
    printf("║    0 = Setup (WiFi)    5 = Settings                  ║\n");
    printf("║    1 = Idle            6 = Temperature               ║\n");
    printf("║    2 = Home            7 = Scale                     ║\n");
    printf("║    3 = Brewing         8 = Cloud                     ║\n");
    printf("║    4 = Complete        9 = Alarm                     ║\n");
    printf("║    O = OTA Update      P = Splash                    ║\n");
    printf("║                                                      ║\n");
    printf("║  ESC = Exit                                          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_E("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }
    
    window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISPLAY_WIDTH, DISPLAY_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        LOG_E("SDL_CreateWindow Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT);
    
    fb = (lv_color_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t));
    memset(fb, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t));
    
    // Initialize LVGL
    lv_init();
    
    // Display driver
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[DISPLAY_WIDTH * 40];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISPLAY_WIDTH * 40);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Encoder input driver
    static lv_indev_drv_t enc_drv;
    lv_indev_drv_init(&enc_drv);
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = encoder_read_cb;
    lv_indev_t* enc_indev = lv_indev_drv_register(&enc_drv);
    
    // Create encoder group
    lv_group_t* group = lv_group_create();
    lv_indev_set_group(enc_indev, group);
    lv_group_set_default(group);
    
    // Initialize theme
    theme_init();
    
    // Initialize UI (creates all screens)
    if (!ui.begin()) {
        LOG_E("Failed to initialize UI");
        return 1;
    }
    
    // Add round display mask
    create_round_mask();
    
    // Start at home screen
    ui.showScreen(SCREEN_HOME);
    ui.update(mock_state);
    
    LOG_I("Simulator running!");
    
    // Main loop
    while (handle_sdl_events()) {
        update_mock_state();
        lv_timer_handler();
        platform_delay(5);
    }
    
    // Cleanup
    free(fb);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
