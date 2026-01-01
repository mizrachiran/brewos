#ifndef MSGPACK_HELPER_H
#define MSGPACK_HELPER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>

/**
 * Lightweight MessagePack encoder for ESP32
 * Optimized for status broadcast messages
 * 
 * MessagePack format reference:
 * - Fixmap: 0x80-0x8F (maps with 0-15 pairs)
 * - Map16: 0xDE (16-bit size)
 * - Map32: 0xDF (32-bit size)
 * - Fixstr: 0xA0-0xBF (strings 0-31 bytes)
 * - Str8: 0xD9 (8-bit length)
 * - Str16: 0xDA (16-bit length)
 * - Str32: 0xDB (32-bit length)
 * - Nil: 0xC0
 * - False: 0xC2, True: 0xC3
 * - Float32: 0xCA, Float64: 0xCB
 * - Uint8: 0xCC, Uint16: 0xCD, Uint32: 0xCE, Uint64: 0xCF
 * - Int8: 0xD0, Int16: 0xD1, Int32: 0xD2, Int64: 0xD3
 */
class MessagePackHelper {
public:
    /**
     * Serialize JSON document to MessagePack binary format
     * @param doc ArduinoJson document (must be an object)
     * @param buffer Output buffer (must be pre-allocated)
     * @param bufferSize Size of output buffer
     * @return Number of bytes written, or 0 on error
     */
    static size_t serialize(const JsonDocument& doc, uint8_t* buffer, size_t bufferSize);
    
    /**
     * Estimate MessagePack size (rough estimate, ~50-60% of JSON)
     */
    static size_t estimateSize(const JsonDocument& doc);

};

#endif // MSGPACK_HELPER_H

