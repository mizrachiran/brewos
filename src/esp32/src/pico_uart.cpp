#include "pico_uart.h"
#include "config.h"

PicoUART::PicoUART(HardwareSerial& serial)
    : _serial(serial)
    , _packetCallback(nullptr)
    , _rxState(RxState::WAIT_START)
    , _rxIndex(0)
    , _rxLength(0)
    , _txSeq(0)
    , _packetsReceived(0)
    , _packetErrors(0)
    , _lastPacketTime(0)
    , _connected(false)
    , _paused(false)
    , _backoffUntil(0) {
}

void PicoUART::begin() {
    LOG_I("Initializing Pico UART at %d baud", PICO_UART_BAUD);
    
    // Configure RX pin with pull-down to prevent floating when Pico is not connected
    // This prevents noise from being interpreted as data
    pinMode(PICO_UART_RX_PIN, INPUT_PULLDOWN);
    
    // Initialize UART
    Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
    
    // Initialize control pins
    // PICO_RUN_PIN: GPIO20 (screen variant) or GPIO4 (no-screen variant)
    // WEIGHT_STOP_PIN: GPIO19 (screen variant) or GPIO6 (no-screen variant)
    pinMode(PICO_RUN_PIN, OUTPUT);
    digitalWrite(PICO_RUN_PIN, HIGH);      // HIGH = Pico running (LOW = reset)
    
    pinMode(WEIGHT_STOP_PIN, OUTPUT);
    digitalWrite(WEIGHT_STOP_PIN, LOW);    // LOW = no weight stop signal
    
    LOG_I("Pico UART initialized. TX=%d, RX=%d", PICO_UART_TX_PIN, PICO_UART_RX_PIN);
    LOG_I("Pico RUN pin: GPIO%d", PICO_RUN_PIN);
}

void PicoUART::loop() {
    // Skip processing if paused (during OTA)
    // This prevents the packet handler from consuming bootloader ACK bytes
    // Note: We do NOT drain the buffer here because OTA process (executePicoOTA)
    // actively reads from Serial1 while PicoUART is paused. Draining would
    // steal data from the OTA process.
    if (_paused) {
        return;
    }
    
    // Update connection status first (safe operation)
    if (_lastPacketTime > 0) {
        _connected = (millis() - _lastPacketTime) < 2000;
    } else {
        _connected = false;
    }
    
    // WORKAROUND: Use Serial1 directly instead of _serial reference
    // The reference seems to get corrupted on ESP32-S3
    int avail = Serial1.available();
    if (avail > 0) {
        while (Serial1.available()) {
            uint8_t byte = Serial1.read();
            processByte(byte);
        }
    }
}

void PicoUART::processByte(uint8_t byte) {
    switch (_rxState) {
        case RxState::WAIT_START:
            if (byte == PROTOCOL_SYNC_BYTE) {
                _rxIndex = 0;
                _rxState = RxState::GOT_TYPE;
            }
            break;
            
        case RxState::GOT_TYPE:
            _rxBuffer[_rxIndex++] = byte;  // type
            _rxState = RxState::GOT_LENGTH;
            break;
            
        case RxState::GOT_LENGTH:
            _rxBuffer[_rxIndex++] = byte;  // length
            _rxLength = byte;
            _rxState = RxState::GOT_SEQ;
            break;
            
        case RxState::GOT_SEQ:
            _rxBuffer[_rxIndex++] = byte;  // seq
            if (_rxLength > 0) {
                _rxState = RxState::READING_PAYLOAD;
            } else {
                _rxState = RxState::READING_CRC;
            }
            break;
            
        case RxState::READING_PAYLOAD:
            _rxBuffer[_rxIndex++] = byte;
            if (_rxIndex >= 3 + _rxLength) {
                _rxState = RxState::READING_CRC;
            }
            break;
            
        case RxState::READING_CRC:
            _rxBuffer[_rxIndex++] = byte;
            if (_rxIndex >= 3 + _rxLength + 2) {
                processPacket();
                _rxState = RxState::WAIT_START;
            }
            break;
    }
    
    // Safety: reset if buffer overflow
    if (_rxIndex >= sizeof(_rxBuffer)) {
        _rxState = RxState::WAIT_START;
        _rxIndex = 0;
        _packetErrors++;
    }
}

void PicoUART::processPacket() {
    // Extract packet fields
    PicoPacket packet;
    packet.type = _rxBuffer[0];
    packet.length = _rxBuffer[1];
    packet.seq = _rxBuffer[2];
    
    // Copy payload
    if (packet.length > 0 && packet.length <= sizeof(packet.payload)) {
        memcpy(packet.payload, &_rxBuffer[3], packet.length);
    }
    
    // Extract CRC (little-endian)
    uint16_t receivedCRC = _rxBuffer[3 + packet.length] | 
                          (_rxBuffer[4 + packet.length] << 8);
    
    // Calculate expected CRC
    uint16_t expectedCRC = calculateCRC(_rxBuffer, 3 + packet.length);
    
    if (receivedCRC == expectedCRC) {
        packet.valid = true;
        packet.crc = receivedCRC;
        _packetsReceived++;
        _lastPacketTime = millis();
        
        // Call callback if set
        if (_packetCallback) {
            _packetCallback(packet);
        }
    } else {
        packet.valid = false;
        _packetErrors++;
        LOG_W("Packet CRC error: received=0x%04X, expected=0x%04X", 
              receivedCRC, expectedCRC);
    }
}

bool PicoUART::sendPacket(uint8_t type, const uint8_t* payload, uint8_t length) {
    if (length > PROTOCOL_MAX_PAYLOAD) {
        LOG_E("Packet payload too large: %d (max: %d)", length, PROTOCOL_MAX_PAYLOAD);
        return false;
    }
    
    // Buffer size: header (4) + max payload (64) + CRC (2) = 70 bytes
    uint8_t buffer[PROTOCOL_MAX_PACKET];
    uint8_t idx = 0;
    
    // Build packet
    buffer[idx++] = PROTOCOL_SYNC_BYTE;
    buffer[idx++] = type;
    buffer[idx++] = length;
    buffer[idx++] = _txSeq++;
    
    // Copy payload
    if (length > 0 && payload != nullptr) {
        memcpy(&buffer[idx], payload, length);
        idx += length;
    }
    
    // Calculate and append CRC
    uint16_t crc = calculateCRC(&buffer[1], 3 + length);
    buffer[idx++] = crc & 0xFF;
    buffer[idx++] = (crc >> 8) & 0xFF;
    
    // Send
    size_t written = Serial1.write(buffer, idx);
    
    return written == idx;
}

bool PicoUART::sendPing() {
    uint32_t timestamp = millis();
    return sendPacket(MSG_PING, (uint8_t*)&timestamp, sizeof(timestamp));
}

bool PicoUART::sendCommand(uint8_t cmdType, const uint8_t* data, uint8_t len) {
    // Check if we're in backoff period (non-blocking NACK handling)
    if (_backoffUntil > 0 && millis() < _backoffUntil) {
        // Still in backoff period - defer command
        // Log only at debug level to avoid spam
        LOG_D("Command 0x%02X deferred - backoff active for %lu ms", 
              cmdType, _backoffUntil - millis());
        return false;
    }
    
    // Clear backoff once we're past the deadline
    _backoffUntil = 0;
    
    return sendPacket(cmdType, data, len);
}

bool PicoUART::requestConfig() {
    return sendPacket(MSG_CMD_GET_CONFIG, nullptr, 0);
}

bool PicoUART::requestBootInfo() {
    return sendPacket(MSG_CMD_GET_BOOT, nullptr, 0);
}

bool PicoUART::sendHandshake() {
    uint8_t handshake[6] = {
        1,  // protocol_version_major
        1,  // protocol_version_minor
        0,  // capabilities
        3,  // max_retry_count
        (uint8_t)(1000 & 0xFF),      // ack_timeout_ms low byte
        (uint8_t)((1000 >> 8) & 0xFF) // ack_timeout_ms high byte
    };
    return sendPacket(MSG_HANDSHAKE, handshake, 6);
}

bool PicoUART::enterBootloader() {
    // ⚠️ HARDWARE BOOTLOADER IS NOT AVAILABLE
    // The Pico's BOOTSEL button connects to QSPI_SS which is NOT exposed on the 40-pin header.
    // There is no way to enter the USB bootloader remotely.
    //
    // For OTA updates, use: sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)
    // This triggers the SOFTWARE bootloader in the Pico firmware.
    //
    // For recovery (bricked Pico), user must physically:
    // 1. Hold BOOTSEL button on Pico
    // 2. Press RUN button (or power cycle)
    // 3. Release BOOTSEL - Pico appears as USB drive "RPI-RP2"
    // 4. Drag-drop .uf2 firmware file
    
    LOG_W("enterBootloader() - Hardware bootloader not available!");
    LOG_W("Pico BOOTSEL (QSPI_SS) is not on the 40-pin header.");
    LOG_I("Use software bootloader: sendCommand(MSG_CMD_BOOTLOADER)");
    
    // Just reset the Pico - won't enter USB bootloader, will boot normally
    resetPico();
    return false;
}

void PicoUART::resetPico() {
    LOG_I("Resetting Pico via GPIO%d...", PICO_RUN_PIN);
    
    // Pull RUN pin LOW to reset Pico (PICO_RUN_PIN)
    digitalWrite(PICO_RUN_PIN, LOW);
    delay(100);
    digitalWrite(PICO_RUN_PIN, HIGH);  // Release - Pico will boot
    
    LOG_I("Pico reset complete");
}

void PicoUART::holdBootsel(bool hold) {
    // ⚠️ THIS FUNCTION HAS NO EFFECT
    // Pico's BOOTSEL (QSPI_SS) is not accessible from the 40-pin header.
    // Function kept for API compatibility only.
    (void)hold;  // Suppress unused parameter warning
}

void PicoUART::setWeightStop(bool active) {
    // Set WEIGHT_STOP signal (HIGH = stop brew, LOW = normal)
    // This signal goes to Pico GPIO21 via J15 Pin 7
    digitalWrite(WEIGHT_STOP_PIN, active ? HIGH : LOW);
}

uint16_t PicoUART::calculateCRC(const uint8_t* data, size_t length) {
    // CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF)
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

bool PicoUART::waitForBootloaderAck(uint32_t timeoutMs) {
    // Bootloader sends 4-byte ACK: 0xB0 0x07 0xAC 0x4B ("BOOT-ACK" pattern)
    // This unique sequence cannot be confused with protocol packets (which start with 0xAA)
    // The bootloader ACK comes AFTER the protocol ACK packet, so we may see:
    // 1. Protocol ACK packet (0xAA ... with CRC)
    // 2. Raw bootloader ACK (0xB0 0x07 0xAC 0x4B)
    
    static const uint8_t BOOT_ACK[] = {0xB0, 0x07, 0xAC, 0x4B};  // "BOOT-ACK"
    static const size_t BOOT_ACK_LEN = sizeof(BOOT_ACK);
    
    unsigned long startTime = millis();
    uint8_t index = 0;
    int bytesRead = 0;
    
    // Reset packet state machine to avoid interference
    RxState savedState = _rxState;
    _rxState = RxState::WAIT_START;
    
    LOG_I("Waiting for bootloader ACK (0xB0 0x07 0xAC 0x4B)...");
    
    // Look for the 4-byte ACK pattern in incoming data
    while ((millis() - startTime) < timeoutMs) {
        if (Serial1.available()) {
            uint8_t byte = Serial1.read();
            bytesRead++;
            
            if (byte == BOOT_ACK[index]) {
                index++;
                if (index == BOOT_ACK_LEN) {
                    // Got complete bootloader ACK!
                    LOG_I("Bootloader ACK received after %d bytes, %lu ms", 
                          bytesRead, millis() - startTime);
                    _rxState = savedState;
                    return true;
                }
            } else if (byte == BOOT_ACK[0]) {
                // Start of new potential match
                index = 1;
            } else {
                // Reset matching
                index = 0;
            }
        } else {
            delay(1);
        }
    }
    
    _rxState = savedState;
    LOG_E("Bootloader ACK timeout after %d ms (read %d bytes total)", timeoutMs, bytesRead);
    return false;
}

size_t PicoUART::streamFirmwareChunk(const uint8_t* data, size_t len, uint32_t chunkNumber) {
    // Stream firmware chunk in bootloader protocol format
    // Format: [MAGIC: 0x55AA] [CHUNK_NUM: 4 bytes LE] [SIZE: 2 bytes LE] [DATA] [CHECKSUM: 1 byte]
    
    if (len > 256) {  // Reasonable limit for chunk size
        LOG_E("Chunk too large: %d", len);
        return 0;
    }
    
    // Build chunk header
    uint8_t header[8];
    header[0] = 0x55;  // Magic byte 1
    header[1] = 0xAA;  // Magic byte 2
    header[2] = chunkNumber & 0xFF;
    header[3] = (chunkNumber >> 8) & 0xFF;
    header[4] = (chunkNumber >> 16) & 0xFF;
    header[5] = (chunkNumber >> 24) & 0xFF;
    header[6] = len & 0xFF;
    header[7] = (len >> 8) & 0xFF;
    
    // Calculate checksum (XOR of all data bytes)
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    
    // Send header
    size_t written = Serial1.write(header, 8);
    if (written != 8) {
        LOG_E("Failed to write chunk header");
        return 0;
    }
    
    // Send data
    written = Serial1.write(data, len);
    if (written != len) {
        LOG_E("Failed to write chunk data: %d/%d", written, len);
        return 0;
    }
    
    // Send checksum
    written = Serial1.write(&checksum, 1);
    if (written != 1) {
        LOG_E("Failed to write checksum");
        return 0;
    }
    
    // Flush to ensure data is sent
    Serial1.flush();
    
    return len;
}

