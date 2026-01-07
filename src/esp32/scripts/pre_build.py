# Pre-build script for PlatformIO
# Patches third-party libraries to fix build warnings with ESP-IDF 5.x
# Also syncs and lints WiFi setup page HTML
#
# Note: NimBLE-Arduino 1.4.x has macro redefinition issues with ESP-IDF 5.x
# because it defines CONFIG_BT_NIMBLE_* macros without checking if they're
# already defined by sdkconfig.h. This script patches the library after
# it's installed.

Import("env")
import os
import sys
import subprocess
from pathlib import Path

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
    
    # Check if already patched (look for our marker comment)
    if "Patched by pre_build.py" in content or "ESP-IDF 5.x compatibility" in content:
        return
    
    # First, check if file is already broken (has extra #endif statements)
    # If so, fix it by removing extra #endif statements
    if_count = len([l for l in content.split('\n') if l.strip().startswith('#if')])
    endif_count = len([l for l in content.split('\n') if l.strip() == '#endif'])
    
    if endif_count > if_count:
        print("[pre_build] Detected corrupted nimconfig.h with extra #endif statements")
        print("[pre_build] Fixing by removing extra #endif statements...")
        # Fix by removing extra #endif statements that don't match an #if
        lines = content.split('\n')
        fixed_lines = []
        if_stack = 0
        for line in lines:
            stripped = line.strip()
            if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                if_stack += 1
                fixed_lines.append(line)
            elif stripped == "#endif":
                if if_stack > 0:
                    if_stack -= 1
                    fixed_lines.append(line)
                # Skip extra #endif that don't match an #if
            else:
                fixed_lines.append(line)
        content = '\n'.join(fixed_lines)
        print(f"[pre_build] Removed {endif_count - if_count} extra #endif statements")
        # Write the fixed content immediately
        with open(nimconfig_path, "w") as f:
            f.write(content)
    
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
        # HCI Event Buffer size - handle the actual structure with if/else
        # Match the full block including the if/else structure
        ('/** @brief HCI Event Buffer size */\n#if CONFIG_BT_NIMBLE_EXT_ADV || CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 257\n#else\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif',
         '/** @brief HCI Event Buffer size */\n#ifndef CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE\n#if CONFIG_BT_NIMBLE_EXT_ADV || CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 257\n#else\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif\n#endif'),
        # Fallback for older version without PERIODIC_ADV
        ('/** @brief HCI Event Buffer size */\n#if CONFIG_BT_NIMBLE_EXT_ADV\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif',
         '/** @brief HCI Event Buffer size */\n#ifndef CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE\n#if CONFIG_BT_NIMBLE_EXT_ADV\n#  define CONFIG_BT_NIMBLE_HCI_EVT_BUF_SIZE 70\n#endif\n#endif'),
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
        # Add marker comment to indicate successful patching
        # Insert after the first line (usually a comment or #ifndef guard)
        lines = content.split('\n')
        if len(lines) > 0:
            # Find a good place to insert the marker (after initial comments/guards)
            insert_pos = 0
            for i, line in enumerate(lines):
                if line.strip() and not line.strip().startswith('#'):
                    insert_pos = i
                    break
                if i > 10:  # Don't search too far
                    insert_pos = 5
                    break
            
            lines.insert(insert_pos, "/* Patched by pre_build.py for ESP-IDF 5.x compatibility */")
            content = '\n'.join(lines)
        
        with open(nimconfig_path, "w") as f:
            f.write(content)
        print("[pre_build] Patched NimBLE-Arduino for ESP-IDF 5.x compatibility")

def sync_wifi_setup_page():
    """
    Sync WiFi setup page HTML to header file and lint it.
    This ensures the header is always up-to-date before compilation.
    """
    # Get project directory (where platformio.ini is located)
    project_dir = Path(env.get("PROJECT_DIR", "."))
    script_dir = project_dir / "scripts"
    html_file = project_dir / "wifi_setup_page_dev.html"
    sync_script = project_dir / "update_wifi_setup_header.py"
    lint_script = script_dir / "lint_wifi_setup.py"
    
    # Skip if HTML file doesn't exist (not an error - might be building without it)
    if not html_file.exists():
        return
    
    # Skip if sync script doesn't exist
    if not sync_script.exists():
        print("[pre_build] Warning: WiFi setup sync script not found, skipping")
        return
    
    print("[pre_build] Syncing WiFi setup page...")
    
    # Run lint first (warnings only, don't fail build)
    if lint_script.exists():
        try:
            result = subprocess.run(
                [sys.executable, str(lint_script)],
                cwd=project_dir,
                capture_output=True,
                text=True,
                timeout=10
            )
            if result.returncode != 0:
                print("[pre_build] WiFi setup lint warnings:")
                print(result.stdout)
                if result.stderr:
                    print(result.stderr)
        except Exception as e:
            print(f"[pre_build] Warning: Lint check failed: {e}")
    
    # Run sync script
    try:
        result = subprocess.run(
            [sys.executable, str(sync_script)],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            print("[pre_build] ✓ WiFi setup page synced")
        else:
            print(f"[pre_build] Error syncing WiFi setup page:")
            print(result.stderr)
            # Don't fail the build, but warn
            print("[pre_build] Warning: Continuing build with potentially outdated WiFi setup page")
    except Exception as e:
        print(f"[pre_build] Warning: WiFi setup sync failed: {e}")
        print("[pre_build] Continuing build...")

def ensure_psram_config():
    """
    Ensure PSRAM is configured for Octal mode for ESP32-S3 N16R8 module.
    This creates/updates sdkconfig.defaults if building for noscreen variant.
    """
    env_name = env.get("PIOENV", "")
    
    # Only apply PSRAM config for noscreen variant
    if "noscreen" not in env_name:
        return
    
    project_dir = Path(env.get("PROJECT_DIR", "."))
    sdkconfig_defaults = project_dir / "sdkconfig.defaults"
    
    # Use noscreen-specific config file
    sdkconfig_env_specific = project_dir / "sdkconfig.defaults.esp32s3-noscreen"
    
    if sdkconfig_env_specific.exists():
        # Copy environment-specific config to sdkconfig.defaults
        import shutil
        shutil.copy2(sdkconfig_env_specific, sdkconfig_defaults)
        print(f"[pre_build] Applied PSRAM config from {sdkconfig_env_specific.name}")

# Run patch immediately when script loads (before library compilation)
patch_nimble_config()

# Ensure PSRAM config is applied
ensure_psram_config()

# Sync WiFi setup page (runs before compilation)
sync_wifi_setup_page()

