"""Binary sensor platform for BrewOS."""
from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
    BinarySensorEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, ICON_COFFEE
from .coordinator import BrewOSCoordinator


@dataclass(frozen=True)
class BrewOSBinarySensorEntityDescription(BinarySensorEntityDescription):
    """Describes BrewOS binary sensor entity."""

    value_key: str | None = None


BINARY_SENSOR_DESCRIPTIONS: tuple[BrewOSBinarySensorEntityDescription, ...] = (
    BrewOSBinarySensorEntityDescription(
        key="is_brewing",
        name="Brewing",
        device_class=BinarySensorDeviceClass.RUNNING,
        icon=ICON_COFFEE,
    ),
    BrewOSBinarySensorEntityDescription(
        key="is_heating",
        name="Heating",
        device_class=BinarySensorDeviceClass.HEAT,
        icon="mdi:fire",
    ),
    BrewOSBinarySensorEntityDescription(
        key="ready",
        name="Machine Ready",
        value_key="state",  # Check if state == "ready"
        icon="mdi:check-circle",
    ),
    BrewOSBinarySensorEntityDescription(
        key="water_low",
        name="Water Low",
        device_class=BinarySensorDeviceClass.PROBLEM,
        icon="mdi:water-off",
    ),
    BrewOSBinarySensorEntityDescription(
        key="alarm_active",
        name="Alarm",
        device_class=BinarySensorDeviceClass.PROBLEM,
        icon="mdi:alert",
    ),
    BrewOSBinarySensorEntityDescription(
        key="pico_connected",
        name="Pico Connected",
        device_class=BinarySensorDeviceClass.CONNECTIVITY,
        icon="mdi:chip",
    ),
    BrewOSBinarySensorEntityDescription(
        key="scale_connected",
        name="Scale Connected",
        device_class=BinarySensorDeviceClass.CONNECTIVITY,
        icon="mdi:bluetooth",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS binary sensors."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSBinarySensor(coordinator, description, entry)
        for description in BINARY_SENSOR_DESCRIPTIONS
    )


class BrewOSBinarySensor(CoordinatorEntity[BrewOSCoordinator], BinarySensorEntity):
    """BrewOS binary sensor entity."""

    entity_description: BrewOSBinarySensorEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: BrewOSBinarySensorEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the binary sensor."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    @property
    def is_on(self) -> bool | None:
        """Return True if the binary sensor is on."""
        desc = self.entity_description
        
        # Special handling for "ready" state
        if desc.key == "ready":
            return self.coordinator.data.get("state") == "ready"
        
        return self.coordinator.data.get(desc.key, False)

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

