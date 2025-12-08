"""BrewOS Espresso Machine Integration for Home Assistant."""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import Platform
from homeassistant.core import HomeAssistant
from homeassistant.helpers import device_registry as dr

from .const import DOMAIN, CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX
from .coordinator import BrewOSCoordinator

_LOGGER = logging.getLogger(__name__)

PLATFORMS: list[Platform] = [
    Platform.SENSOR,
    Platform.BINARY_SENSOR,
    Platform.SWITCH,
    Platform.BUTTON,
    Platform.NUMBER,
    Platform.SELECT,
]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up BrewOS from a config entry."""
    hass.data.setdefault(DOMAIN, {})
    
    topic_prefix = entry.data.get(CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX)
    device_id = entry.data.get("device_id", "")
    
    coordinator = BrewOSCoordinator(hass, topic_prefix, device_id)
    await coordinator.async_setup()
    
    hass.data[DOMAIN][entry.entry_id] = coordinator
    
    # Register device
    device_registry = dr.async_get(hass)
    device_registry.async_get_or_create(
        config_entry_id=entry.entry_id,
        identifiers={(DOMAIN, device_id or topic_prefix)},
        manufacturer="BrewOS",
        model="ECM Controller",
        name=entry.title or "BrewOS Espresso Machine",
        sw_version=coordinator.data.get("sw_version", "unknown"),
    )
    
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    if unload_ok := await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        coordinator = hass.data[DOMAIN].pop(entry.entry_id)
        await coordinator.async_shutdown()
    
    return unload_ok

