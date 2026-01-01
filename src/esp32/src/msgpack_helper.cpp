#include "msgpack_helper.h"
#include <ArduinoJson.h>

// Helper functions (static to this file)
static bool writeBytes(const uint8_t* data, size_t len, uint8_t* buffer, size_t bufferSize, size_t& offset);
static bool writeByte(uint8_t byte, uint8_t* buffer, size_t bufferSize, size_t& offset);
static size_t packValue(JsonVariantConst value, uint8_t* buffer, size_t bufferSize, size_t& offset);
static size_t packString(const char* str, uint8_t* buffer, size_t bufferSize, size_t& offset);
static size_t packObject(JsonObjectConst obj, uint8_t* buffer, size_t bufferSize, size_t& offset);
static size_t packArray(JsonArrayConst arr, uint8_t* buffer, size_t bufferSize, size_t& offset);

static bool writeBytes(const uint8_t* data, size_t len, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    if (offset + len > bufferSize) {
        return false;
    }
    memcpy(buffer + offset, data, len);
    offset += len;
    return true;
}

static bool writeByte(uint8_t byte, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    if (offset >= bufferSize) {
        return false;
    }
    buffer[offset++] = byte;
    return true;
}

static size_t packString(const char* str, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    if (!str) {
        return writeByte(0xC0, buffer, bufferSize, offset) ? 1 : 0; // nil
    }
    
    size_t len = strlen(str);
    size_t startOffset = offset;
    
    if (len <= 31) {
        // Fixstr: 0xA0-0xBF
        if (!writeByte(0xA0 | len, buffer, bufferSize, offset)) return 0;
    } else if (len <= 255) {
        // Str8: 0xD9
        if (!writeByte(0xD9, buffer, bufferSize, offset)) return 0;
        if (!writeByte(len & 0xFF, buffer, bufferSize, offset)) return 0;
    } else if (len <= 65535) {
        // Str16: 0xDA
        if (!writeByte(0xDA, buffer, bufferSize, offset)) return 0;
        if (!writeBytes((uint8_t*)&len, 2, buffer, bufferSize, offset)) return 0;
        // Swap bytes for big-endian (MessagePack uses big-endian)
        uint8_t* lenPtr = buffer + offset - 2;
        uint8_t tmp = lenPtr[0];
        lenPtr[0] = lenPtr[1];
        lenPtr[1] = tmp;
    } else {
        // Str32: 0xDB
        if (!writeByte(0xDB, buffer, bufferSize, offset)) return 0;
        uint32_t len32 = len;
        if (!writeBytes((uint8_t*)&len32, 4, buffer, bufferSize, offset)) return 0;
        // Swap bytes for big-endian
        uint8_t* lenPtr = buffer + offset - 4;
        for (int i = 0; i < 2; i++) {
            uint8_t tmp = lenPtr[i];
            lenPtr[i] = lenPtr[3-i];
            lenPtr[3-i] = tmp;
        }
    }
    
    if (!writeBytes((const uint8_t*)str, len, buffer, bufferSize, offset)) return 0;
    return offset - startOffset;
}

static size_t packValue(JsonVariantConst value, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    size_t startOffset = offset;
    
    if (value.isNull()) {
        if (!writeByte(0xC0, buffer, bufferSize, offset)) return 0; // nil
        return 1;
    }
    
    if (value.is<bool>()) {
        if (!writeByte(value.as<bool>() ? 0xC3 : 0xC2, buffer, bufferSize, offset)) return 0;
        return 1;
    }
    
    if (value.is<int>()) {
        int32_t v = value.as<int32_t>();
        if (v >= 0 && v <= 127) {
            // Positive fixint: 0x00-0x7F
            if (!writeByte(v & 0x7F, buffer, bufferSize, offset)) return 0;
        } else if (v >= -32 && v < 0) {
            // Negative fixint: 0xE0-0xFF
            if (!writeByte(0xE0 | (v & 0x1F), buffer, bufferSize, offset)) return 0;
        } else if (v >= -128 && v < 128) {
            // Int8: 0xD0
            if (!writeByte(0xD0, buffer, bufferSize, offset)) return 0;
            if (!writeByte(v & 0xFF, buffer, bufferSize, offset)) return 0;
        } else if (v >= -32768 && v < 32768) {
            // Int16: 0xD1
            if (!writeByte(0xD1, buffer, bufferSize, offset)) return 0;
            int16_t v16 = v;
            if (!writeBytes((uint8_t*)&v16, 2, buffer, bufferSize, offset)) return 0;
            // Big-endian swap
            uint8_t* vPtr = buffer + offset - 2;
            uint8_t tmp = vPtr[0];
            vPtr[0] = vPtr[1];
            vPtr[1] = tmp;
        } else {
            // Int32: 0xD2
            if (!writeByte(0xD2, buffer, bufferSize, offset)) return 0;
            if (!writeBytes((uint8_t*)&v, 4, buffer, bufferSize, offset)) return 0;
            // Big-endian swap
            uint8_t* vPtr = buffer + offset - 4;
            for (int i = 0; i < 2; i++) {
                uint8_t tmp = vPtr[i];
                vPtr[i] = vPtr[3-i];
                vPtr[3-i] = tmp;
            }
        }
        return offset - startOffset;
    }
    
    if (value.is<unsigned int>() || value.is<uint32_t>()) {
        uint32_t v = value.as<uint32_t>();
        if (v <= 127) {
            // Positive fixint
            if (!writeByte(v & 0x7F, buffer, bufferSize, offset)) return 0;
        } else if (v <= 255) {
            // Uint8: 0xCC
            if (!writeByte(0xCC, buffer, bufferSize, offset)) return 0;
            if (!writeByte(v & 0xFF, buffer, bufferSize, offset)) return 0;
        } else if (v <= 65535) {
            // Uint16: 0xCD
            if (!writeByte(0xCD, buffer, bufferSize, offset)) return 0;
            uint16_t v16 = v;
            if (!writeBytes((uint8_t*)&v16, 2, buffer, bufferSize, offset)) return 0;
            // Big-endian swap
            uint8_t* vPtr = buffer + offset - 2;
            uint8_t tmp = vPtr[0];
            vPtr[0] = vPtr[1];
            vPtr[1] = tmp;
        } else {
            // Uint32: 0xCE
            if (!writeByte(0xCE, buffer, bufferSize, offset)) return 0;
            if (!writeBytes((uint8_t*)&v, 4, buffer, bufferSize, offset)) return 0;
            // Big-endian swap
            uint8_t* vPtr = buffer + offset - 4;
            for (int i = 0; i < 2; i++) {
                uint8_t tmp = vPtr[i];
                vPtr[i] = vPtr[3-i];
                vPtr[3-i] = tmp;
            }
        }
        return offset - startOffset;
    }
    
    if (value.is<float>() || value.is<double>()) {
        double v = value.as<double>();
        // Use float32 if value fits, otherwise float64
        float f = (float)v;
        if (f == v && f >= -3.4028235e38 && f <= 3.4028235e38) {
            // Float32: 0xCA
            if (!writeByte(0xCA, buffer, bufferSize, offset)) return 0;
            if (!writeBytes((uint8_t*)&f, 4, buffer, bufferSize, offset)) return 0;
            // Big-endian swap for float
            uint8_t* fPtr = buffer + offset - 4;
            for (int i = 0; i < 2; i++) {
                uint8_t tmp = fPtr[i];
                fPtr[i] = fPtr[3-i];
                fPtr[3-i] = tmp;
            }
        } else {
            // Float64: 0xCB
            if (!writeByte(0xCB, buffer, bufferSize, offset)) return 0;
            if (!writeBytes((uint8_t*)&v, 8, buffer, bufferSize, offset)) return 0;
            // Big-endian swap for double
            uint8_t* dPtr = buffer + offset - 8;
            for (int i = 0; i < 4; i++) {
                uint8_t tmp = dPtr[i];
                dPtr[i] = dPtr[7-i];
                dPtr[7-i] = tmp;
            }
        }
        return offset - startOffset;
    }
    
    if (value.is<const char*>() || value.is<String>()) {
        const char* str = value.as<const char*>();
        return packString(str, buffer, bufferSize, offset);
    }
    
    if (value.is<JsonObjectConst>()) {
        return packObject(value.as<JsonObjectConst>(), buffer, bufferSize, offset);
    }
    
    if (value.is<JsonArrayConst>()) {
        return packArray(value.as<JsonArrayConst>(), buffer, bufferSize, offset);
    }
    
    // Fallback: convert to string
    String str = value.as<String>();
    return packString(str.c_str(), buffer, bufferSize, offset);
}

static size_t packObject(JsonObjectConst obj, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    size_t startOffset = offset;
    
    // Count non-null pairs
    size_t count = 0;
    for (JsonPairConst pair : obj) {
        if (!pair.value().isNull()) {
            count++;
        }
    }
    
    // Write map header
    if (count <= 15) {
        // Fixmap: 0x80-0x8F
        if (!writeByte(0x80 | count, buffer, bufferSize, offset)) return 0;
    } else if (count <= 65535) {
        // Map16: 0xDE
        if (!writeByte(0xDE, buffer, bufferSize, offset)) return 0;
        uint16_t count16 = count;
        if (!writeBytes((uint8_t*)&count16, 2, buffer, bufferSize, offset)) return 0;
        // Big-endian swap
        uint8_t* countPtr = buffer + offset - 2;
        uint8_t tmp = countPtr[0];
        countPtr[0] = countPtr[1];
        countPtr[1] = tmp;
    } else {
        // Map32: 0xDF
        if (!writeByte(0xDF, buffer, bufferSize, offset)) return 0;
        uint32_t count32 = count;
        if (!writeBytes((uint8_t*)&count32, 4, buffer, bufferSize, offset)) return 0;
        // Big-endian swap
        uint8_t* countPtr = buffer + offset - 4;
        for (int i = 0; i < 2; i++) {
            uint8_t tmp = countPtr[i];
            countPtr[i] = countPtr[3-i];
            countPtr[3-i] = tmp;
        }
    }
    
    // Write key-value pairs
    for (JsonPairConst pair : obj) {
        if (!pair.value().isNull()) {
            // Pack key
            if (!packString(pair.key().c_str(), buffer, bufferSize, offset)) return 0;
            // Pack value
            if (!packValue(pair.value(), buffer, bufferSize, offset)) return 0;
        }
    }
    
    return offset - startOffset;
}

static size_t packArray(JsonArrayConst arr, uint8_t* buffer, size_t bufferSize, size_t& offset) {
    size_t startOffset = offset;
    size_t count = arr.size();
    
    // Write array header
    if (count <= 15) {
        // Fixarray: 0x90-0x9F
        if (!writeByte(0x90 | count, buffer, bufferSize, offset)) return 0;
    } else if (count <= 65535) {
        // Array16: 0xDC
        if (!writeByte(0xDC, buffer, bufferSize, offset)) return 0;
        uint16_t count16 = count;
        if (!writeBytes((uint8_t*)&count16, 2, buffer, bufferSize, offset)) return 0;
        // Big-endian swap
        uint8_t* countPtr = buffer + offset - 2;
        uint8_t tmp = countPtr[0];
        countPtr[0] = countPtr[1];
        countPtr[1] = tmp;
    } else {
        // Array32: 0xDD
        if (!writeByte(0xDD, buffer, bufferSize, offset)) return 0;
        uint32_t count32 = count;
        if (!writeBytes((uint8_t*)&count32, 4, buffer, bufferSize, offset)) return 0;
        // Big-endian swap
        uint8_t* countPtr = buffer + offset - 4;
        for (int i = 0; i < 2; i++) {
            uint8_t tmp = countPtr[i];
            countPtr[i] = countPtr[3-i];
            countPtr[3-i] = tmp;
        }
    }
    
    // Write array elements
    for (JsonVariantConst value : arr) {
        if (!packValue(value, buffer, bufferSize, offset)) return 0;
    }
    
    return offset - startOffset;
}

size_t MessagePackHelper::serialize(const JsonDocument& doc, uint8_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return 0;
    }
    
    size_t offset = 0;
    
    // ArduinoJson v7: JsonDocument can be accessed as JsonVariantConst
    // We need to check the type and handle accordingly
    JsonVariantConst variant = doc.as<JsonVariantConst>();
    
    if (variant.is<JsonObjectConst>()) {
        JsonObjectConst obj = variant.as<JsonObjectConst>();
        size_t written = packObject(obj, buffer, bufferSize, offset);
        return written > 0 ? offset : 0;
    } else if (variant.is<JsonArrayConst>()) {
        JsonArrayConst arr = variant.as<JsonArrayConst>();
        size_t written = packArray(arr, buffer, bufferSize, offset);
        return written > 0 ? offset : 0;
    } else {
        // Single value
        size_t written = packValue(variant, buffer, bufferSize, offset);
        return written > 0 ? offset : 0;
    }
}

size_t MessagePackHelper::estimateSize(const JsonDocument& doc) {
    // Rough estimate: MessagePack is typically 50-60% of JSON size
    size_t jsonSize = measureJson(doc);
    return (jsonSize * 55) / 100; // 55% of JSON size (conservative)
}

