"""Select platform for BrewOS."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components.select import SelectEntity, SelectEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import (
    DOMAIN,
    MODE_ECO,
    MODE_ON,
    MODE_STANDBY,
    HEATING_STRATEGIES,
    STRATEGY_VALUE_MAP,
    STRATEGY_NAME_MAP,
)
from .coordinator import BrewOSCoordinator


@dataclass(frozen=True)
class BrewOSSelectEntityDescription(SelectEntityDescription):
    """Describes BrewOS select entity."""

    command: str = ""
    command_param: str = ""
    is_strategy: bool = False


SELECT_DESCRIPTIONS: tuple[BrewOSSelectEntityDescription, ...] = (
    BrewOSSelectEntityDescription(
        key="mode",
        name="Machine Mode",
        icon="mdi:coffee-maker-outline",
        options=[MODE_STANDBY, MODE_ON, MODE_ECO],
        command="set_mode",
        command_param="mode",
    ),
    BrewOSSelectEntityDescription(
        key="heating_strategy",
        name="Heating Strategy",
        icon="mdi:fire",
        options=HEATING_STRATEGIES,
        command="set_heating_strategy",
        command_param="strategy",
        is_strategy=True,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS selects."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSSelect(coordinator, description, entry)
        for description in SELECT_DESCRIPTIONS
    )


class BrewOSSelect(CoordinatorEntity[BrewOSCoordinator], SelectEntity):
    """BrewOS select entity."""

    entity_description: BrewOSSelectEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: BrewOSSelectEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the select entity."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    @property
    def current_option(self) -> str | None:
        """Return the current selected option."""
        desc = self.entity_description
        value = self.coordinator.data.get(desc.key)
        
        # For heating strategy, convert numeric value to string
        if desc.is_strategy and isinstance(value, int):
            return STRATEGY_VALUE_MAP.get(value, "sequential")
        
        return value or MODE_STANDBY

    async def async_select_option(self, option: str) -> None:
        """Change the selected option."""
        desc = self.entity_description
        
        # For heating strategy, convert string to numeric value
        if desc.is_strategy:
            value = STRATEGY_NAME_MAP.get(option, 1)
            await self.coordinator.async_send_command(desc.command, **{desc.command_param: value})
        else:
            await self.coordinator.async_send_command(desc.command, **{desc.command_param: option})

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

