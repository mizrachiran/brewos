"""Switch platform for BrewOS."""
from __future__ import annotations

from typing import Any

from homeassistant.components.switch import SwitchEntity, SwitchEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, MODE_ON, MODE_STANDBY
from .coordinator import BrewOSCoordinator


SWITCH_DESCRIPTIONS: tuple[SwitchEntityDescription, ...] = (
    SwitchEntityDescription(
        key="power",
        name="Power",
        icon="mdi:power",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS switches."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSSwitch(coordinator, description, entry)
        for description in SWITCH_DESCRIPTIONS
    )


class BrewOSSwitch(CoordinatorEntity[BrewOSCoordinator], SwitchEntity):
    """BrewOS switch entity."""

    entity_description: SwitchEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: SwitchEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the switch."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    @property
    def is_on(self) -> bool:
        """Return True if switch is on."""
        mode = self.coordinator.data.get("mode", MODE_STANDBY)
        return mode != MODE_STANDBY

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on the switch."""
        await self.coordinator.async_send_command("set_mode", mode=MODE_ON)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the switch."""
        await self.coordinator.async_send_command("set_mode", mode=MODE_STANDBY)

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

