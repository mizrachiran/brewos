# Pre-build script for PlatformIO
# Patches third-party libraries to fix build warnings with ESP-IDF 5.x
#
# Note: NimBLE-Arduino 1.4.x has macro redefinition issues with ESP-IDF 5.x
# because it defines CONFIG_BT_NIMBLE_* macros without checking if they're
# already defined by sdkconfig.h. This script patches the library after
# it's installed.

Import("env")
import os

def patch_nimble_config():
    """
    Patch NimBLE-Arduino nimconfig.h to add #ifndef guards around macros
    that conflict with ESP-IDF 5.x sdkconfig.h definitions.
    """
    nimconfig_path = os.path.join(
        env.get("PROJECT_LIBDEPS_DIR", ".pio/libdeps"),
        env.get("PIOENV", "esp32s3"),
        "NimBLE-Arduino",
        "src",
        "nimconfig.h"
    )
    
    if not os.path.exists(nimconfig_path):
        return
    
    with open(nimconfig_path, "r") as f:
        content = f.read()
    
    # Check if already patched (look for our comment)
    if "ESP-IDF 5.x compatibility" in content:
        return
    
    # Replace unconditional #defines with guarded versions
    replacements = [
        # Role macros - wrap existing conditional with additional sdkconfig check
        ('#ifndef CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED\n#define CONFIG_BT_NIMBLE_ROLE_CENTRAL\n#endif',
         '#if !defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED) && !defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL)\n#define CONFIG_BT_NIMBLE_ROLE_CENTRAL\n#endif'),
        ('#ifndef CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED\n#define CONFIG_BT_NIMBLE_ROLE_OBSERVER\n#endif',
         '#if !defined(CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED) && !defined(CONFIG_BT_NIMBLE_ROLE_OBSERVER)\n#define CONFIG_BT_NIMBLE_ROLE_OBSERVER\n#endif'),
        ('#ifndef CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED\n#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL\n#endif',
         '#if !defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED) && !defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)\n#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL\n#endif'),
        ('#ifndef CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED\n#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER\n#endif',
         '#if !defined(CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED) && !defined(CONFIG_BT_NIMBLE_ROLE_BROADCASTER)\n#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER\n#endif'),
        # Buffer count/size macros - add #ifndef guards
        ('/** @brief ACL Buffer count */\n#define CONFIG_BT_NIMBLE_ACL_BUF_COUNT 12',
         '/** @brief ACL Buffer count */\n#ifndef CONFIG_BT_NIMBLE_ACL_BUF_COUNT\n#define CONFIG_BT_NIMBLE_ACL_BUF_COUNT 12\n#endif'),
        ('/** @brief ACL Buffer size */\n#define CONFIG_BT_NIMBLE_ACL_BUF_SIZE 255',
         '/** @brief ACL Buffer size */\n#ifndef CONFIG_BT_NIMBLE_ACL_BUF_SIZE\n#define CONFIG_BT_NIMBLE_ACL_BUF_SIZE 255\n#endif'),
        ('/** @brief HCI Event Buffer size */\n#if CONFIG_BT_NIMBLE_EXT_ADV',
         '/** @brief HCI Event Buffer size */\n#ifndef CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE\n#if CONFIG_BT_NIMBLE_EXT_ADV'),
        ('#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif',
         '#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif\n#endif'),
        ('/** @brief Number of high priority HCI event buffers */\n#define CONFIG_BT_NIMBLE_HCI_EVT_HI_BUF_COUNT 30',
         '/** @brief Number of high priority HCI event buffers */\n#ifndef CONFIG_BT_NIMBLE_HCI_EVT_HI_BUF_COUNT\n#define CONFIG_BT_NIMBLE_HCI_EVT_HI_BUF_COUNT 30\n#endif'),
        ('/** @brief Number of low priority HCI event buffers */\n#define CONFIG_BT_NIMBLE_HCI_EVT_LO_BUF_COUNT 8',
         '/** @brief Number of low priority HCI event buffers */\n#ifndef CONFIG_BT_NIMBLE_HCI_EVT_LO_BUF_COUNT\n#define CONFIG_BT_NIMBLE_HCI_EVT_LO_BUF_COUNT 8\n#endif'),
    ]
    
    modified = False
    for old, new in replacements:
        if old in content:
            content = content.replace(old, new)
            modified = True
    
    if modified:
        with open(nimconfig_path, "w") as f:
            f.write(content)
        print("[pre_build] Patched NimBLE-Arduino for ESP-IDF 5.x compatibility")

# Run patch immediately when script loads (before library compilation)
patch_nimble_config()

