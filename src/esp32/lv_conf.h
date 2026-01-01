/**
 * LVGL Configuration for BrewOS ESP32-S3 Display
 * 
 * Target: UEDX48480021-MD80E (480x480 Round IPS)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. 
 * - Set to 1 for SPI displays (data sent serially needs big-endian)
 * - Set to 0 for parallel RGB displays (native format)
 * Disabled for both simulator and RGB panel displays */
#ifdef SIMULATOR
    #define LV_COLOR_16_SWAP 0
#else
    /* RGB parallel panel uses native byte order - no swap needed */
    #define LV_COLOR_16_SWAP 0
#endif

/* Enable more complex drawing routines for gradient, etc. */
#define LV_DRAW_COMPLEX 1

/* Enable anti-aliasing */
#define LV_ANTIALIAS 1

/*====================
   MEMORY SETTINGS
 *====================*/

#ifdef SIMULATOR
    /* Simulator: use standard malloc (no PSRAM) */
    #define LV_MEM_SIZE (128 * 1024U)  /* 128KB */
    #define LV_MEM_CUSTOM 0
#else
    /* ESP32: Use custom malloc/free to route LVGL allocations to PSRAM
     * This frees ~128KB of internal RAM for WiFi/SSL operations
     * 
     * PSRAM Usage Strategy:
     * - LVGL frame buffers and graphics objects are allocated in PSRAM
     * - Internal RAM is reserved for:
     *   - SSL handshake buffers (~16KB contiguous blocks needed)
     *   - WiFi stack buffers
     *   - Critical system objects (WiFiManager, MQTTClient, etc.)
     * - Large JSON documents (>1KB) should also use PSRAM via SpiRamJsonDocument
     */
    #define LV_MEM_CUSTOM 1
    #define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
    #define LV_MEM_CUSTOM_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
    #define LV_MEM_CUSTOM_FREE(p) heap_caps_free(p)
    #define LV_MEM_CUSTOM_REALLOC(p, size) heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM)
#endif

/* Use the standard `memcpy` and `memset` instead of LVGL's own functions */
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in milliseconds */
#define LV_DISP_DEF_REFR_PERIOD 16  /* ~60fps */

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 30

/* Use a custom tick source */
#define LV_TICK_CUSTOM 1
#ifdef SIMULATOR
    #define LV_TICK_CUSTOM_INCLUDE "simulator/lv_tick.h"
#else
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#endif
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*====================
   FEATURE CONFIGURATION
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/* Enable complex rectangle drawing with gradients, shadows, etc. */
#define LV_DRAW_COMPLEX 1

/* Enable shadow drawing */
#define LV_SHADOW_CACHE_SIZE 0

/* Maximum number of cached circle data */
#define LV_CIRCLE_CACHE_SIZE 4

/*-------------
 * GPU
 *-----------*/

/* Use ESP32's DMA2D accelerator */
#define LV_USE_GPU_ESP32_DMA2D 0  /* Not available on ESP32-S3 */

/*-------------
 * Logging
 *-----------*/

/* Enable logging */
/* Disabled for production to improve frame rates - re-enable for debugging */
#define LV_USE_LOG 0

#if LV_USE_LOG
    /* Log level */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    
    /* Print logs using printf */
    #define LV_LOG_PRINTF 1
    
    /* Enable/disable trace logs */
    #define LV_LOG_TRACE_MEM 0
    #define LV_LOG_TRACE_TIMER 0
    #define LV_LOG_TRACE_INDEV 0
    #define LV_LOG_TRACE_DISP_REFR 0
    #define LV_LOG_TRACE_EVENT 0
    #define LV_LOG_TRACE_OBJ_CREATE 0
    #define LV_LOG_TRACE_LAYOUT 0
    #define LV_LOG_TRACE_ANIM 0
#endif

/*-------------
 * Asserts
 *-----------*/

/* Enable asserts */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/*-------------
 * Others
 *-----------*/

/* Reference to user data in objects and other things */
#define LV_USE_USER_DATA 1

/* Garbage Collector settings */
#define LV_ENABLE_GC 0

/*====================
 * COMPILER SETTINGS
 *====================*/

/* For big endian systems set to 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Define a custom attribute to `lv_tick_inc` function */
#define LV_ATTRIBUTE_TICK_INC

/* Define a custom attribute to `lv_timer_handler` function */
#define LV_ATTRIBUTE_TIMER_HANDLER

/* Define a custom attribute to `lv_disp_flush_ready` function */
#define LV_ATTRIBUTE_FLUSH_READY

/* Required alignment size for buffers */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 4

/* Will be added where memories needs to be aligned (with -Os data might not be aligned to boundary by default) */
#define LV_ATTRIBUTE_MEM_ALIGN

/* Attribute to mark large constant arrays */
#define LV_ATTRIBUTE_LARGE_CONST

/* Compiler prefix for a big array declaration in RAM */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Place performance critical functions into a faster memory (e.g RAM) */
#define LV_ATTRIBUTE_FAST_MEM

/* Prefix variables that are used in GPU accelerated operations */
#define LV_ATTRIBUTE_DMA

/* Export integer constant to binding */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Prefix all global extern data with this */
#define LV_ATTRIBUTE_EXTERN_DATA

/*====================
 *   FONT USAGE
 *====================*/

/* Montserrat fonts with ASCII range and some symbols */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1  /* Large numbers for timer/temp */

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Enable handling large fonts (requires more memory) */
#define LV_FONT_FMT_TXT_LARGE 0

/* Enable subpixel rendering */
#define LV_FONT_SUBPX_BGR 0

/*====================
 *   TEXT SETTINGS
 *====================*/

/* Select character encoding */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Can break (wrap) texts on these characters */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* If a word is at least this long, will break wherever "prettiest" */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum number of characters in a long word to put on a line before a break */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum number of characters in a long word to put on a line after a break */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* The control character to use for signalling text recoloring */
#define LV_TXT_COLOR_CMD "#"

/* Support bidirectional texts */
#define LV_USE_BIDI 0

/* Enable Arabic/Persian processing */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
 *   WIDGET USAGE
 *====================*/

/* Documentation for widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      0

/*====================
 * EXTRA COMPONENTS
 *====================*/

/*-----------
 * Widgets
 *----------*/

#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      1  /* For temperature graphs */
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        1  /* Status indicators */
#define LV_USE_LIST       1
#define LV_USE_MENU       1  /* Settings menu */
#define LV_USE_METER      1  /* Gauge-like displays */
#define LV_USE_MSGBOX     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    1  /* Temperature/weight input */
#define LV_USE_SPINNER    1  /* Loading indicator */
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*-----------
 * Themes
 *----------*/

/* A simple, impressive and very complete theme */
#define LV_USE_THEME_DEFAULT 1

#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1
    
    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 1
    
    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* A very simple theme that is a good starting point for a custom theme */
#define LV_USE_THEME_BASIC 0

/* A theme designed for monochrome displays */
#define LV_USE_THEME_MONO 0

/*-----------
 * Layouts
 *----------*/

/* A layout similar to Flexbox in CSS */
#define LV_USE_FLEX 1

/* A layout similar to Grid in CSS */
#define LV_USE_GRID 1

/*-----------
 * Others
 *----------*/

/* Enable file explorer */
#define LV_USE_FILE_EXPLORER 0

/* Enable IME */
#define LV_USE_IME_PINYIN 0

/* Enable snapshot widget */
#define LV_USE_SNAPSHOT 0

/* Enable Monkey test */
#define LV_USE_MONKEY 0

/* Enable gridnav */
#define LV_USE_GRIDNAV 0

/* Enable fragment */
#define LV_USE_FRAGMENT 0

/* 1: Enable Observer */
#define LV_USE_OBSERVER 0

#endif /* LV_CONF_H */

