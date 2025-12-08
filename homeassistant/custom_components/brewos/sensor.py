"""Sensor platform for BrewOS."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import (
    UnitOfEnergy,
    UnitOfElectricCurrent,
    UnitOfElectricPotential,
    UnitOfMass,
    UnitOfPower,
    UnitOfPressure,
    UnitOfTemperature,
    UnitOfTime,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, ICON_COFFEE, ICON_COUNTER, ICON_SCALE, ICON_TIMER
from .coordinator import BrewOSCoordinator


@dataclass(frozen=True)
class BrewOSSensorEntityDescription(SensorEntityDescription):
    """Describes BrewOS sensor entity."""

    value_fn: callable = lambda data: data.get(key)


SENSOR_DESCRIPTIONS: tuple[BrewOSSensorEntityDescription, ...] = (
    # Temperature sensors
    BrewOSSensorEntityDescription(
        key="brew_temp",
        name="Brew Temperature",
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:thermometer",
    ),
    BrewOSSensorEntityDescription(
        key="brew_setpoint",
        name="Brew Setpoint",
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:thermometer-lines",
    ),
    BrewOSSensorEntityDescription(
        key="steam_temp",
        name="Steam Temperature",
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:thermometer-high",
    ),
    BrewOSSensorEntityDescription(
        key="steam_setpoint",
        name="Steam Setpoint",
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:thermometer-lines",
    ),
    # Pressure
    BrewOSSensorEntityDescription(
        key="pressure",
        name="Brew Pressure",
        native_unit_of_measurement=UnitOfPressure.BAR,
        device_class=SensorDeviceClass.PRESSURE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:gauge",
    ),
    # Scale/Shot sensors
    BrewOSSensorEntityDescription(
        key="scale_weight",
        name="Scale Weight",
        native_unit_of_measurement=UnitOfMass.GRAMS,
        device_class=SensorDeviceClass.WEIGHT,
        state_class=SensorStateClass.MEASUREMENT,
        icon=ICON_SCALE,
    ),
    BrewOSSensorEntityDescription(
        key="flow_rate",
        name="Flow Rate",
        native_unit_of_measurement="g/s",
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:water-outline",
    ),
    BrewOSSensorEntityDescription(
        key="shot_duration",
        name="Shot Duration",
        native_unit_of_measurement=UnitOfTime.SECONDS,
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.MEASUREMENT,
        icon=ICON_TIMER,
    ),
    BrewOSSensorEntityDescription(
        key="shot_weight",
        name="Shot Weight",
        native_unit_of_measurement=UnitOfMass.GRAMS,
        device_class=SensorDeviceClass.WEIGHT,
        state_class=SensorStateClass.MEASUREMENT,
        icon=ICON_COFFEE,
    ),
    BrewOSSensorEntityDescription(
        key="target_weight",
        name="Target Weight",
        native_unit_of_measurement=UnitOfMass.GRAMS,
        device_class=SensorDeviceClass.WEIGHT,
        icon="mdi:target",
    ),
    # Statistics
    BrewOSSensorEntityDescription(
        key="shots_today",
        name="Shots Today",
        native_unit_of_measurement="shots",
        state_class=SensorStateClass.TOTAL_INCREASING,
        icon=ICON_COUNTER,
    ),
    BrewOSSensorEntityDescription(
        key="total_shots",
        name="Total Shots",
        native_unit_of_measurement="shots",
        state_class=SensorStateClass.TOTAL_INCREASING,
        icon="mdi:coffee-maker",
    ),
    BrewOSSensorEntityDescription(
        key="kwh_today",
        name="Energy Today",
        native_unit_of_measurement=UnitOfEnergy.KILO_WATT_HOUR,
        device_class=SensorDeviceClass.ENERGY,
        state_class=SensorStateClass.TOTAL_INCREASING,
        icon="mdi:lightning-bolt",
    ),
    # Power meter
    BrewOSSensorEntityDescription(
        key="power",
        name="Power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:flash",
    ),
    BrewOSSensorEntityDescription(
        key="voltage",
        name="Voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:sine-wave",
    ),
    BrewOSSensorEntityDescription(
        key="current",
        name="Current",
        native_unit_of_measurement=UnitOfElectricCurrent.AMPERE,
        device_class=SensorDeviceClass.CURRENT,
        state_class=SensorStateClass.MEASUREMENT,
        icon="mdi:current-ac",
    ),
    # Machine state (text sensor)
    BrewOSSensorEntityDescription(
        key="state",
        name="Machine State",
        icon="mdi:state-machine",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS sensors."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSSensor(coordinator, description, entry)
        for description in SENSOR_DESCRIPTIONS
    )


class BrewOSSensor(CoordinatorEntity[BrewOSCoordinator], SensorEntity):
    """BrewOS sensor entity."""

    entity_description: BrewOSSensorEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: BrewOSSensorEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    @property
    def native_value(self) -> Any:
        """Return the sensor value."""
        return self.coordinator.data.get(self.entity_description.key)

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

