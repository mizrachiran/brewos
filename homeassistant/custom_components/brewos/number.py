"""Number platform for BrewOS."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components.number import (
    NumberEntity,
    NumberEntityDescription,
    NumberMode,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import UnitOfMass, UnitOfTemperature
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import BrewOSCoordinator


@dataclass(frozen=True)
class BrewOSNumberEntityDescription(NumberEntityDescription):
    """Describes BrewOS number entity."""

    command: str = ""
    command_param: str = ""
    command_extra: dict | None = None


NUMBER_DESCRIPTIONS: tuple[BrewOSNumberEntityDescription, ...] = (
    BrewOSNumberEntityDescription(
        key="brew_setpoint",
        name="Brew Temperature Target",
        icon="mdi:thermometer",
        native_min_value=85.0,
        native_max_value=100.0,
        native_step=0.5,
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        mode=NumberMode.SLIDER,
        command="set_temp",
        command_param="temp",
        command_extra={"boiler": "brew"},
    ),
    BrewOSNumberEntityDescription(
        key="steam_setpoint",
        name="Steam Temperature Target",
        icon="mdi:thermometer-high",
        native_min_value=120.0,
        native_max_value=160.0,
        native_step=1.0,
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        mode=NumberMode.SLIDER,
        command="set_temp",
        command_param="temp",
        command_extra={"boiler": "steam"},
    ),
    BrewOSNumberEntityDescription(
        key="target_weight",
        name="Target Weight",
        icon="mdi:target",
        native_min_value=18.0,
        native_max_value=100.0,
        native_step=0.5,
        native_unit_of_measurement=UnitOfMass.GRAMS,
        mode=NumberMode.SLIDER,
        command="set_target_weight",
        command_param="weight",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS numbers."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSNumber(coordinator, description, entry)
        for description in NUMBER_DESCRIPTIONS
    )


class BrewOSNumber(CoordinatorEntity[BrewOSCoordinator], NumberEntity):
    """BrewOS number entity."""

    entity_description: BrewOSNumberEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: BrewOSNumberEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the number entity."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    @property
    def native_value(self) -> float | None:
        """Return the current value."""
        return self.coordinator.data.get(self.entity_description.key)

    async def async_set_native_value(self, value: float) -> None:
        """Set the value."""
        desc = self.entity_description
        kwargs = {desc.command_param: value}
        if desc.command_extra:
            kwargs.update(desc.command_extra)
        await self.coordinator.async_send_command(desc.command, **kwargs)

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

