"""Button platform for BrewOS."""
from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components.button import ButtonEntity, ButtonEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, ICON_COFFEE, ICON_SCALE
from .coordinator import BrewOSCoordinator


@dataclass(frozen=True)
class BrewOSButtonEntityDescription(ButtonEntityDescription):
    """Describes BrewOS button entity."""

    command: str = ""
    command_kwargs: dict | None = None


BUTTON_DESCRIPTIONS: tuple[BrewOSButtonEntityDescription, ...] = (
    BrewOSButtonEntityDescription(
        key="start_brew",
        name="Start Brew",
        icon=ICON_COFFEE,
        command="brew_start",
    ),
    BrewOSButtonEntityDescription(
        key="stop_brew",
        name="Stop Brew",
        icon="mdi:stop",
        command="brew_stop",
    ),
    BrewOSButtonEntityDescription(
        key="tare_scale",
        name="Tare Scale",
        icon=ICON_SCALE,
        command="tare",
    ),
    BrewOSButtonEntityDescription(
        key="enter_eco",
        name="Enter Eco Mode",
        icon="mdi:leaf",
        command="enter_eco",
    ),
    BrewOSButtonEntityDescription(
        key="exit_eco",
        name="Exit Eco Mode",
        icon="mdi:lightning-bolt",
        command="exit_eco",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up BrewOS buttons."""
    coordinator: BrewOSCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities(
        BrewOSButton(coordinator, description, entry)
        for description in BUTTON_DESCRIPTIONS
    )


class BrewOSButton(CoordinatorEntity[BrewOSCoordinator], ButtonEntity):
    """BrewOS button entity."""

    entity_description: BrewOSButtonEntityDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: BrewOSCoordinator,
        description: BrewOSButtonEntityDescription,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry.data.get("device_id", entry.entry_id))},
        }

    async def async_press(self) -> None:
        """Handle button press."""
        desc = self.entity_description
        kwargs = desc.command_kwargs or {}
        await self.coordinator.async_send_command(desc.command, **kwargs)

    @property
    def available(self) -> bool:
        """Return True if entity is available."""
        return self.coordinator.available

