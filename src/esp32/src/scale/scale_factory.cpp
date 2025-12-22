/**
 * BrewOS Scale Utilities Implementation
 * 
 * Scale type detection and utilities
 */

#include "scale/scale_interface.h"
#include "config.h"

// =============================================================================
// Scale Type Detection
// =============================================================================

scale_type_t detectScaleType(const char* name) {
    if (!name || strlen(name) == 0) return SCALE_TYPE_UNKNOWN;
    
    String n = String(name);
    n.toLowerCase();
    
    // Acaia scales - various models
    if (n.startsWith("acaia") || 
        n.startsWith("lunar") || 
        n.startsWith("pearl") || 
        n.startsWith("pyxis") ||
        n.startsWith("cinco") || 
        n.startsWith("orion")) {
        return SCALE_TYPE_ACAIA;
    }
    
    // Bookoo Themis scales (use Acaia-compatible protocol)
    if (n.startsWith("bookoo") || 
        n.startsWith("themis") ||
        n.indexOf("themis") >= 0) {
        return SCALE_TYPE_BOOKOO;
    }
    
    // Felicita scales
    if (n.startsWith("felicita") || 
        n.startsWith("arc") || 
        n.startsWith("parallel") || 
        n.indexOf("incline") >= 0) {
        return SCALE_TYPE_FELICITA;
    }
    
    // Decent Scale
    if (n.startsWith("decent") || 
        n.startsWith("de1") ||
        n.indexOf("decent scale") >= 0) {
        return SCALE_TYPE_DECENT;
    }
    
    // Timemore
    if (n.startsWith("timemore") || 
        n.indexOf("black mirror") >= 0 ||
        n.indexOf("basic") >= 0) {  // Timemore Basic
        return SCALE_TYPE_TIMEMORE;
    }
    
    // Hiroia
    if (n.startsWith("hiroia") || 
        n.startsWith("jimmy")) {
        return SCALE_TYPE_HIROIA;
    }
    
    // Skale (uses similar protocol to Felicita)
    if (n.startsWith("skale")) {
        return SCALE_TYPE_FELICITA;
    }
    
    // Brewista (similar to generic)
    if (n.startsWith("brewista")) {
        return SCALE_TYPE_GENERIC_WSS;
    }
    
    return SCALE_TYPE_UNKNOWN;
}

const char* getScaleTypeName(scale_type_t type) {
    switch (type) {
        case SCALE_TYPE_ACAIA:      return "Acaia";
        case SCALE_TYPE_FELICITA:   return "Felicita";
        case SCALE_TYPE_DECENT:     return "Decent";
        case SCALE_TYPE_TIMEMORE:   return "Timemore";
        case SCALE_TYPE_HIROIA:     return "Hiroia";
        case SCALE_TYPE_BOOKOO:     return "Bookoo";
        case SCALE_TYPE_GENERIC_WSS: return "Generic";
        default:                    return "Unknown";
    }
}
