"""Constants for the BrewOS integration."""
from __future__ import annotations

from typing import Final

DOMAIN: Final = "brewos"

# Configuration
CONF_TOPIC_PREFIX: Final = "topic_prefix"
DEFAULT_TOPIC_PREFIX: Final = "brewos"

# MQTT Topics
TOPIC_STATUS: Final = "status"
TOPIC_POWER: Final = "power"
TOPIC_STATISTICS: Final = "statistics"
TOPIC_SHOT: Final = "shot"
TOPIC_COMMAND: Final = "command"
TOPIC_AVAILABILITY: Final = "availability"

# Device Classes
DEVICE_CLASS_TEMPERATURE: Final = "temperature"
DEVICE_CLASS_PRESSURE: Final = "pressure"
DEVICE_CLASS_POWER: Final = "power"
DEVICE_CLASS_ENERGY: Final = "energy"
DEVICE_CLASS_DURATION: Final = "duration"

# Units
UNIT_CELSIUS: Final = "°C"
UNIT_BAR: Final = "bar"
UNIT_GRAM: Final = "g"
UNIT_GRAM_PER_SEC: Final = "g/s"
UNIT_WATT: Final = "W"
UNIT_KWH: Final = "kWh"
UNIT_VOLT: Final = "V"
UNIT_AMPERE: Final = "A"

# Machine States
STATE_STANDBY: Final = "standby"
STATE_HEATING: Final = "heating"
STATE_READY: Final = "ready"
STATE_BREWING: Final = "brewing"
STATE_STEAMING: Final = "steaming"
STATE_COOLDOWN: Final = "cooldown"
STATE_FAULT: Final = "fault"

# Machine Modes
MODE_STANDBY: Final = "standby"
MODE_ON: Final = "on"
MODE_ECO: Final = "eco"

# Heating Strategies
STRATEGY_BREW_ONLY: Final = "brew_only"
STRATEGY_SEQUENTIAL: Final = "sequential"
STRATEGY_PARALLEL: Final = "parallel"
STRATEGY_SMART_STAGGER: Final = "smart_stagger"

HEATING_STRATEGIES: Final = [
    STRATEGY_BREW_ONLY,
    STRATEGY_SEQUENTIAL,
    STRATEGY_PARALLEL,
    STRATEGY_SMART_STAGGER,
]

# Strategy value mapping (index = value from ESP32)
STRATEGY_VALUE_MAP: Final = {
    0: STRATEGY_BREW_ONLY,
    1: STRATEGY_SEQUENTIAL,
    2: STRATEGY_PARALLEL,
    3: STRATEGY_SMART_STAGGER,
}

STRATEGY_NAME_MAP: Final = {
    STRATEGY_BREW_ONLY: 0,
    STRATEGY_SEQUENTIAL: 1,
    STRATEGY_PARALLEL: 2,
    STRATEGY_SMART_STAGGER: 3,
}

# Icons
ICON_COFFEE: Final = "mdi:coffee"
ICON_TEMPERATURE: Final = "mdi:thermometer"
ICON_PRESSURE: Final = "mdi:gauge"
ICON_SCALE: Final = "mdi:scale"
ICON_POWER: Final = "mdi:lightning-bolt"
ICON_TIMER: Final = "mdi:timer"
ICON_COUNTER: Final = "mdi:counter"

