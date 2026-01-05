#!/usr/bin/env python3
"""
Live reload script for WiFi Setup Page development.
Monitors wifi_setup_page_dev.html and automatically syncs to header file on changes.
"""

import os
import sys
import time
import subprocess
from pathlib import Path

# Try to use watchdog for efficient file watching, fallback to polling
try:
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    HAS_WATCHDOG = True
except ImportError:
    HAS_WATCHDOG = False
    print("Note: 'watchdog' not installed. Using polling mode (install with: pip install watchdog)")
    print("      Polling checks every 1 second.\n")

SCRIPT_DIR = Path(__file__).parent
HTML_FILE = SCRIPT_DIR / "wifi_setup_page_dev.html"
HEADER_FILE = SCRIPT_DIR / "include" / "wifi_setup_page.h"
SYNC_SCRIPT = SCRIPT_DIR / "update_wifi_setup_header.py"

class WiFiSetupHandler(FileSystemEventHandler):
    """Handler for file system events"""
    
    def __init__(self):
        self.last_modified = 0
        self.debounce_time = 0.5  # Wait 0.5s after last change before syncing
        
    def on_modified(self, event):
        if event.is_directory:
            return
        
        if Path(event.src_path) == HTML_FILE:
            current_time = time.time()
            # Debounce: only sync if enough time has passed since last modification
            if current_time - self.last_modified > self.debounce_time:
                self.last_modified = current_time
                self.sync_file()
    
    def sync_file(self):
        """Run the sync script"""
        print(f"\n[{time.strftime('%H:%M:%S')}] Detected change in {HTML_FILE.name}")
        print("Syncing to header file...", end=" ", flush=True)
        
        try:
            result = subprocess.run(
                [sys.executable, str(SYNC_SCRIPT)],
                capture_output=True,
                text=True,
                cwd=SCRIPT_DIR
            )
            
            if result.returncode == 0:
                print("✓ Done")
                if result.stdout:
                    print(result.stdout.strip())
            else:
                print("✗ Error")
                if result.stderr:
                    print(result.stderr.strip())
        except Exception as e:
            print(f"✗ Failed: {e}")

def watch_with_polling(html_file, handler):
    """Fallback polling-based file watcher"""
    print(f"Watching {html_file} (polling every 1 second)...")
    print("Press Ctrl+C to stop\n")
    
    last_mtime = 0
    
    try:
        while True:
            if html_file.exists():
                current_mtime = html_file.stat().st_mtime
                if current_mtime != last_mtime:
                    last_mtime = current_mtime
                    time.sleep(handler.debounce_time)  # Debounce
                    handler.sync_file()
            time.sleep(1)  # Poll every second
    except KeyboardInterrupt:
        print("\n\nStopped watching.")

def watch_with_watchdog(html_file, handler):
    """Efficient file watching using watchdog"""
    print(f"Watching {html_file} (using watchdog)...")
    print("Press Ctrl+C to stop\n")
    
    observer = Observer()
    observer.schedule(handler, path=html_file.parent, recursive=False)
    observer.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n\nStopping watcher...")
        observer.stop()
    
    observer.join()

def main():
    if not HTML_FILE.exists():
        print(f"Error: {HTML_FILE} not found")
        sys.exit(1)
    
    if not SYNC_SCRIPT.exists():
        print(f"Error: {SYNC_SCRIPT} not found")
        sys.exit(1)
    
    print("=" * 60)
    print("WiFi Setup Page - Live Reload")
    print("=" * 60)
    print(f"Source: {HTML_FILE}")
    print(f"Target: {HEADER_FILE}")
    print()
    
    # Do an initial sync
    print("Performing initial sync...", end=" ", flush=True)
    handler = WiFiSetupHandler()
    try:
        result = subprocess.run(
            [sys.executable, str(SYNC_SCRIPT)],
            capture_output=True,
            text=True,
            cwd=SCRIPT_DIR
        )
        if result.returncode == 0:
            print("✓ Done\n")
        else:
            print("✗ Error")
            print(result.stderr)
            sys.exit(1)
    except Exception as e:
        print(f"✗ Failed: {e}")
        sys.exit(1)
    
    # Start watching
    if HAS_WATCHDOG:
        watch_with_watchdog(HTML_FILE, handler)
    else:
        watch_with_polling(HTML_FILE, handler)

if __name__ == "__main__":
    main()

