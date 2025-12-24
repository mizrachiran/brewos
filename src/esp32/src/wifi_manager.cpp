#include "wifi_manager.h"
#include "config.h"
#include "lwip/dns.h"  // For direct DNS server configuration
#include "esp_wifi.h"  // For esp_wifi_set_ps()

// Helper to validate function pointers before calling
// On ESP32-S3, valid code addresses are in flash (0x40000000-0x42FFFFFF)
// PSRAM data is at 0x3C000000-0x3DFFFFFF - calling this would crash
static inline bool isValidCodePointer(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    // Valid ESP32-S3 code/data regions:
    // 0x3FF80000 - 0x3FFFFFFF: DRAM (function pointers can reside here)
    // 0x40000000 - 0x4001FFFF: ROM
    // 0x40020000 - 0x40027FFF: IRAM 
    // 0x42000000 - 0x42FFFFFF: Flash mapped code
    return ((addr >= 0x40000000 && addr < 0x43000000) || 
            (addr >= 0x3FF80000 && addr <= 0x3FFFFFFF));
}

// Safe callback invocation with validation
static inline void safeCallback(WiFiEventCallback callback) {
    if (callback && isValidCodePointer((void*)callback)) {
        callback();
    } else if (callback) {
        // Callback pointer is corrupted (pointing to PSRAM or invalid area)
        Serial.printf("[FATAL] Corrupted callback pointer: 0x%08X - NOT calling!\n", (uint32_t)callback);
    }
}

WiFiManager::WiFiManager() 
    : _mode(WiFiManagerMode::DISCONNECTED)
    , _lastConnectAttempt(0)
    , _connectStartTime(0)
    , _onConnected(nullptr)
    , _onDisconnected(nullptr)
    , _onAPStarted(nullptr) {
    // Initialize credential buffers
    _storedSSID[0] = '\0';
    _storedPassword[0] = '\0';
}

void WiFiManager::begin() {
    LOG_I("WiFi Manager starting...");
    
    // Load stored credentials and static IP config
    loadCredentials();
    loadStaticIPConfig();
    
    // Try to connect if we have credentials
    if (hasStoredCredentials()) {
        LOG_I("Found stored WiFi credentials for: %s", _storedSSID);
        connectToWiFi();
    } else {
        LOG_I("No stored credentials, starting AP mode");
        startAP();
    }
}

void WiFiManager::loop() {
    switch (_mode) {
        case WiFiManagerMode::STA_CONNECTING:
            // Check connection status
            if (WiFi.status() == WL_CONNECTED) {
                _mode = WiFiManagerMode::STA_MODE;
                // Get IP using direct format to avoid String allocation
                IPAddress ip = WiFi.localIP();
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                int rssi = WiFi.RSSI();
                LOG_I("WiFi connected! IP: %s, RSSI: %d dBm, Channel: %d", 
                    ipStr, rssi, WiFi.channel());
                
                // Warn if signal is weak (can cause slow SSL and packet loss)
                if (rssi < -70) {
                    LOG_W("Weak WiFi signal! RSSI=%d dBm (should be > -70)", rssi);
                }
                
                // CRITICAL: Aggressively disable power save mode
                // WiFi power save causes massive delays (packets buffered by router for 100ms-DTIM beacon interval)
                // The ESP32 only wakes up periodically in power save mode, causing ping/web timeouts
                WiFi.setSleep(false);
                
                // Use ESP-IDF API directly - this is more reliable than Arduino's setSleep()
                esp_err_t ps_result = esp_wifi_set_ps(WIFI_PS_NONE);
                if (ps_result == ESP_OK) {
                    LOG_I("WiFi power save DISABLED successfully");
                } else {
                    LOG_E("Failed to disable WiFi power save: %d", ps_result);
                }
                
                // Verify the setting
                wifi_ps_type_t ps_type;
                esp_wifi_get_ps(&ps_type);
                LOG_I("WiFi power save state: %d (0=NONE, 1=MIN_MODEM, 2=MAX_MODEM)", ps_type);
                
                // Set Cloudflare DNS directly using lwip for reliable resolution
                ip_addr_t dns1, dns2;
                IP4_ADDR(&dns1.u_addr.ip4, 1, 1, 1, 1);      // Cloudflare primary
                IP4_ADDR(&dns2.u_addr.ip4, 1, 0, 0, 1);      // Cloudflare secondary
                dns1.type = IPADDR_TYPE_V4;
                dns2.type = IPADDR_TYPE_V4;
                dns_setserver(0, &dns1);
                dns_setserver(1, &dns2);
                LOG_I("DNS set to Cloudflare: 1.1.1.1, 1.0.0.1");
                
                safeCallback(_onConnected);
            } 
            else if (millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
                LOG_W("WiFi connection timeout, starting AP mode");
                startAP();
            }
            break;
            
        case WiFiManagerMode::STA_MODE:
            // Check if still connected
            if (WiFi.status() != WL_CONNECTED) {
                LOG_W("WiFi disconnected");
                _mode = WiFiManagerMode::DISCONNECTED;
                safeCallback(_onDisconnected);
                
                // Try to reconnect after interval
                if (millis() - _lastConnectAttempt > WIFI_RECONNECT_INTERVAL) {
                    connectToWiFi();
                }
            }
            break;
            
        case WiFiManagerMode::AP_MODE:
            // AP mode is stable, nothing to do
            break;
            
        case WiFiManagerMode::DISCONNECTED:
            // Try to reconnect if we have credentials
            if (hasStoredCredentials() && 
                millis() - _lastConnectAttempt > WIFI_RECONNECT_INTERVAL) {
                connectToWiFi();
            }
            break;
    }
}

bool WiFiManager::hasStoredCredentials() {
    return strlen(_storedSSID) > 0 && strlen(_storedPassword) > 0;
}

bool WiFiManager::checkCredentials() {
    // Load credentials from NVS (if not already loaded)
    if (_storedSSID[0] == '\0' && _storedPassword[0] == '\0') {
        loadCredentials();
    }
    return hasStoredCredentials();
}

bool WiFiManager::setCredentials(const String& ssid, const String& password) {
    if (ssid.length() == 0 || password.length() < 8) {
        LOG_E("Invalid credentials");
        return false;
    }
    
    saveCredentials(ssid, password);
    strncpy(_storedSSID, ssid.c_str(), sizeof(_storedSSID) - 1);
    _storedSSID[sizeof(_storedSSID) - 1] = '\0';
    strncpy(_storedPassword, password.c_str(), sizeof(_storedPassword) - 1);
    _storedPassword[sizeof(_storedPassword) - 1] = '\0';
    
    LOG_I("Credentials saved for: %s", ssid.c_str());
    return true;
}

void WiFiManager::clearCredentials() {
    _prefs.begin("wifi", false);
    _prefs.clear();
    _prefs.end();
    
    _storedSSID[0] = '\0';
    _storedPassword[0] = '\0';
    
    LOG_I("Credentials cleared");
}

bool WiFiManager::connectToWiFi() {
    if (!hasStoredCredentials()) {
        LOG_E("No credentials to connect with");
        return false;
    }
    
    LOG_I("Connecting to WiFi: %s", _storedSSID);
    
    // Simple WiFi setup - minimal changes to avoid driver issues
    WiFi.mode(WIFI_STA);
    
    // CRITICAL: Force legacy 802.11b/g mode - fixes "connected but no ping" on ESP32-S3
    // 802.11n (WiFi 4) is unstable with display interference, causing zombie connections
    // where router sees device but packets are dropped
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
    LOG_I("WiFi protocol set to 802.11b/g (legacy mode for stability)");
    
    // Moderate TX power - EMI now controlled via GPIO drive strength
    // 17dBm provides good range without excessive power draw
    WiFi.setTxPower(WIFI_POWER_17dBm);
    
    // Disable power save for responsive network
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    // Apply static IP configuration if enabled
    if (_staticIP.enabled) {
        LOG_I("Using static IP: %s", _staticIP.ip.toString().c_str());
        if (!WiFi.config(_staticIP.ip, _staticIP.gateway, _staticIP.subnet, _staticIP.dns1, _staticIP.dns2)) {
            LOG_E("Failed to configure static IP");
        }
    }
    
    // Connect with optimized settings
    WiFi.begin(_storedSSID, _storedPassword);
    
    // Lower MTU to 1400 to prevent fragmentation issues with some ISPs/routers (Cloudflare)
    // This often fixes "start_ssl_client: -1" timeouts
    // Note: Must be called before begin() on some versions, or after on others. 
    // Calling it here (before begin) sets the interface config.
    // We also call it in the connected callback to be sure.
    // Note: Arduino ESP32 2.x doesn't expose setMTU on WiFi directly easily, 
    // but we can use the lwip interface if needed. 
    // For now, we rely on standard behavior or add tcp_mss check if needed.
    
    _mode = WiFiManagerMode::STA_CONNECTING;
    _connectStartTime = millis();
    _lastConnectAttempt = millis();
    
    return true;
}

void WiFiManager::startAP() {
    LOG_I("Starting AP mode: %s", WIFI_AP_SSID);
    
    // Use AP+STA mode to keep existing WiFi connection while starting AP
    // This allows cloud connection to remain active during setup
    WiFiMode_t current_mode = WiFi.getMode();
    if (current_mode == WIFI_STA || current_mode == WIFI_AP_STA) {
        // Already in STA or AP+STA mode - keep STA connection
        WiFi.mode(WIFI_AP_STA);
        LOG_I("Switching to AP+STA mode (keeping existing WiFi connection)");
    } else {
        // No existing connection - use AP only
        WiFi.mode(WIFI_AP);
        LOG_I("Starting in AP-only mode (no existing WiFi connection)");
    }
    
    // For AP mode, use maximum power (19.5dBm) for maximum visibility
    // Station mode uses 11dBm to reduce EMI, but AP needs maximum range
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Maximum power for AP visibility
    
    // Disable WiFi power save for AP mode
    WiFi.setSleep(false);
    
    // Configure AP IP settings
    WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
    
    // Start AP with hidden=false to ensure network is visible
    // Channel 1 is in the standard range (1-11) for best compatibility
    bool ap_started = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, false, WIFI_AP_MAX_CONNECTIONS);
    
    if (!ap_started) {
        LOG_E("Failed to start AP!");
        return;
    }
    
    // Give AP time to initialize and start broadcasting
    delay(100);
    
    // Update mode - if we have STA connection, we're in AP+STA mode
    if (WiFi.status() == WL_CONNECTED) {
        _mode = WiFiManagerMode::STA_MODE;  // Still connected to router
        LOG_I("AP started while maintaining STA connection to: %s", WiFi.SSID().c_str());
    } else {
        _mode = WiFiManagerMode::AP_MODE;  // AP only
    }
    
    // Get AP IP without String allocation
    IPAddress apIP = WiFi.softAPIP();
    char apIPStr[16];
    snprintf(apIPStr, sizeof(apIPStr), "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
    LOG_I("AP started. IP: %s", apIPStr);
    LOG_I("AP SSID: %s, Channel: %d, Power: 19.5dBm", WIFI_AP_SSID, WIFI_AP_CHANNEL);
    
    safeCallback(_onAPStarted);
}

WiFiStatus WiFiManager::getStatus() {
    WiFiStatus status;
    status.mode = _mode;
    status.configured = hasStoredCredentials();
    
    // Static IP info
    status.staticIp = _staticIP.enabled;
    status.gateway = _staticIP.enabled ? _staticIP.gateway.toString() : "";
    status.subnet = _staticIP.enabled ? _staticIP.subnet.toString() : "";
    status.dns1 = _staticIP.enabled ? _staticIP.dns1.toString() : "";
    status.dns2 = _staticIP.enabled ? _staticIP.dns2.toString() : "";
    
    switch (_mode) {
        case WiFiManagerMode::AP_MODE:
            status.ssid = WIFI_AP_SSID;
            status.ip = WiFi.softAPIP().toString();
            status.rssi = 0;
            break;
            
        case WiFiManagerMode::STA_MODE:
            status.ssid = WiFi.SSID();
            status.ip = WiFi.localIP().toString();
            status.rssi = WiFi.RSSI();
            // Update gateway/DNS from actual values when connected via DHCP
            if (!_staticIP.enabled) {
                status.gateway = WiFi.gatewayIP().toString();
                status.subnet = WiFi.subnetMask().toString();
                status.dns1 = WiFi.dnsIP(0).toString();
                status.dns2 = WiFi.dnsIP(1).toString();
            }
            break;
            
        case WiFiManagerMode::STA_CONNECTING:
            status.ssid = _storedSSID;
            status.ip = "Connecting...";
            status.rssi = 0;
            break;
            
        default:
            status.ssid = "";
            status.ip = "";
            status.rssi = 0;
    }
    
    return status;
}

bool WiFiManager::isConnected() {
    return _mode == WiFiManagerMode::STA_MODE || _mode == WiFiManagerMode::AP_MODE;
}

String WiFiManager::getIP() {
    if (_mode == WiFiManagerMode::AP_MODE) {
        return WiFi.softAPIP().toString();
    } else if (_mode == WiFiManagerMode::STA_MODE) {
        return WiFi.localIP().toString();
    }
    return "";
}

void WiFiManager::loadCredentials() {
    // After fresh flash, NVS namespace won't exist - this is expected
    if (!_prefs.begin("wifi", true)) {  // Read-only
        LOG_I("No saved WiFi credentials (fresh flash) - using defaults");
        _storedSSID[0] = '\0';
        _storedPassword[0] = '\0';
        return;
    }
    
    // Load to temporary String, then copy to fixed buffer
    String ssid = _prefs.getString("ssid", "");
    String password = _prefs.getString("password", "");
    _prefs.end();
    
    strncpy(_storedSSID, ssid.c_str(), sizeof(_storedSSID) - 1);
    _storedSSID[sizeof(_storedSSID) - 1] = '\0';
    strncpy(_storedPassword, password.c_str(), sizeof(_storedPassword) - 1);
    _storedPassword[sizeof(_storedPassword) - 1] = '\0';
}

void WiFiManager::saveCredentials(const String& ssid, const String& password) {
    _prefs.begin("wifi", false);  // Read-write
    _prefs.putString("ssid", ssid);
    _prefs.putString("password", password);
    _prefs.end();
}

// =============================================================================
// Static IP Configuration
// =============================================================================

void WiFiManager::setStaticIP(bool enabled, const String& ip, const String& gateway, 
                               const String& subnet, const String& dns1, const String& dns2) {
    _staticIP.enabled = enabled;
    
    if (enabled) {
        _staticIP.ip.fromString(ip);
        _staticIP.gateway.fromString(gateway);
        _staticIP.subnet.fromString(subnet.length() > 0 ? subnet : "255.255.255.0");
        if (dns1.length() > 0) {
            _staticIP.dns1.fromString(dns1);
        } else {
            _staticIP.dns1 = _staticIP.gateway; // Default DNS to gateway
        }
        if (dns2.length() > 0) {
            _staticIP.dns2.fromString(dns2);
        } else {
            _staticIP.dns2 = IPAddress(8, 8, 8, 8); // Google DNS as fallback
        }
        
        LOG_I("Static IP configured: IP=%s, GW=%s, DNS=%s", 
              ip.c_str(), gateway.c_str(), dns1.c_str());
    } else {
        LOG_I("DHCP mode enabled");
    }
    
    saveStaticIPConfig();
}

void WiFiManager::loadStaticIPConfig() {
    // After fresh flash, NVS namespace won't exist - this is expected
    if (!_prefs.begin("wifi", true)) {  // Read-only
        LOG_I("No saved static IP config (fresh flash) - using defaults");
        _staticIP.enabled = false;
        return;
    }
    _staticIP.enabled = _prefs.getBool("static_en", false);
    
    if (_staticIP.enabled) {
        String ip = _prefs.getString("static_ip", "");
        String gw = _prefs.getString("static_gw", "");
        String sn = _prefs.getString("static_sn", "255.255.255.0");
        String dns1 = _prefs.getString("static_dns1", "");
        String dns2 = _prefs.getString("static_dns2", "");
        
        if (ip.length() > 0) _staticIP.ip.fromString(ip);
        if (gw.length() > 0) _staticIP.gateway.fromString(gw);
        if (sn.length() > 0) _staticIP.subnet.fromString(sn);
        if (dns1.length() > 0) _staticIP.dns1.fromString(dns1);
        if (dns2.length() > 0) _staticIP.dns2.fromString(dns2);
        
        LOG_I("Loaded static IP config: %s", ip.c_str());
    }
    
    _prefs.end();
}

void WiFiManager::saveStaticIPConfig() {
    _prefs.begin("wifi", false);  // Read-write
    _prefs.putBool("static_en", _staticIP.enabled);
    
    if (_staticIP.enabled) {
        _prefs.putString("static_ip", _staticIP.ip.toString());
        _prefs.putString("static_gw", _staticIP.gateway.toString());
        _prefs.putString("static_sn", _staticIP.subnet.toString());
        _prefs.putString("static_dns1", _staticIP.dns1.toString());
        _prefs.putString("static_dns2", _staticIP.dns2.toString());
    }
    
    _prefs.end();
}

// =============================================================================
// NTP/Time Functions
// =============================================================================

void WiFiManager::configureNTP(const char* server, int16_t utcOffsetMinutes, bool dstEnabled, int16_t dstOffsetMinutes) {
    strncpy(_ntpServer, server, sizeof(_ntpServer) - 1);
    _utcOffsetSec = utcOffsetMinutes * 60;
    _dstOffsetSec = dstEnabled ? (dstOffsetMinutes * 60) : 0;
    _ntpConfigured = true;
    
    LOG_I("NTP configured: server=%s, UTC offset=%d min, DST=%s (%d min)",
          _ntpServer, utcOffsetMinutes, dstEnabled ? "on" : "off", dstOffsetMinutes);
    
    // Apply configuration immediately if WiFi connected
    if (_mode == WiFiManagerMode::STA_MODE) {
        syncNTP();
    }
}

void WiFiManager::syncNTP() {
    if (!_ntpConfigured) {
        // Use defaults if not configured
        configureNTP("pool.ntp.org", 0, false, 0);
    }
    
    LOG_I("Configuring NTP: %s (UTC%+d)", _ntpServer, _utcOffsetSec / 3600);
    
    // Configure time with timezone offset
    // ESP32 uses POSIX timezone format: GMT-offset (inverted from what you'd expect)
    // For UTC+2: "UTC-2" because POSIX uses the opposite sign
    char tzStr[32];
    int hours = _utcOffsetSec / 3600;
    int mins = abs((_utcOffsetSec % 3600) / 60);
    
    if (_dstOffsetSec > 0) {
        // With DST - format: STD-offset DST for simple rule
        snprintf(tzStr, sizeof(tzStr), "UTC%+d", -hours);
    } else {
        snprintf(tzStr, sizeof(tzStr), "UTC%+d", -hours);
    }
    
    // Set timezone and NTP server
    configTzTime(tzStr, _ntpServer);
    
    LOG_I("NTP sync started, timezone: %s", tzStr);
}

bool WiFiManager::isTimeSynced() {
    time_t now = time(nullptr);
    // If time is after 2020, consider it synced (NTP provides epoch time)
    return now > 1577836800;  // Jan 1, 2020
}

TimeStatus WiFiManager::getTimeStatus() {
    TimeStatus status;
    status.ntpSynced = isTimeSynced();
    status.utcOffset = _utcOffsetSec + _dstOffsetSec;
    
    if (status.ntpSynced) {
        status.currentTime = getFormattedTime();
        
        // Format timezone string
        int totalOffsetMin = status.utcOffset / 60;
        int hours = totalOffsetMin / 60;
        int mins = abs(totalOffsetMin % 60);
        char tz[16];
        if (mins > 0) {
            snprintf(tz, sizeof(tz), "UTC%+d:%02d", hours, mins);
        } else {
            snprintf(tz, sizeof(tz), "UTC%+d", hours);
        }
        status.timezone = tz;
    } else {
        status.currentTime = "Not synced";
        status.timezone = "Unknown";
    }
    
    return status;
}

time_t WiFiManager::getLocalTime() {
    return time(nullptr);
}

String WiFiManager::getFormattedTime(const char* format) {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (!timeinfo) {
        return "Invalid";
    }
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), format, timeinfo);
    return String(buffer);
}

