"""Data coordinator for BrewOS."""
from __future__ import annotations

import json
import logging
from typing import Any

from homeassistant.components import mqtt
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator

from .const import (
    DOMAIN,
    TOPIC_STATUS,
    TOPIC_POWER,
    TOPIC_STATISTICS,
    TOPIC_AVAILABILITY,
    TOPIC_COMMAND,
)

_LOGGER = logging.getLogger(__name__)


class BrewOSCoordinator(DataUpdateCoordinator[dict[str, Any]]):
    """Coordinator to manage BrewOS data from MQTT."""

    def __init__(
        self,
        hass: HomeAssistant,
        topic_prefix: str,
        device_id: str,
    ) -> None:
        """Initialize the coordinator."""
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
        )
        self.topic_prefix = topic_prefix
        self.device_id = device_id
        self._subscriptions: list[Any] = []
        self._available = False
        
        # Initialize data with defaults
        self.data = {
            "state": "standby",
            "mode": "standby",
            "heating_strategy": 1,  # Default to sequential
            "brew_temp": 0.0,
            "brew_setpoint": 93.5,
            "steam_temp": 0.0,
            "steam_setpoint": 145.0,
            "pressure": 0.0,
            "scale_weight": 0.0,
            "flow_rate": 0.0,
            "shot_duration": 0.0,
            "shot_weight": 0.0,
            "target_weight": 36.0,
            "is_brewing": False,
            "is_heating": False,
            "water_low": False,
            "alarm_active": False,
            "pico_connected": False,
            "wifi_connected": True,
            "scale_connected": False,
            "shots_today": 0,
            "total_shots": 0,
            "kwh_today": 0.0,
            "power": 0,
            "voltage": 0,
            "current": 0,
            "available": False,
            "sw_version": "unknown",
        }

    def _build_topic(self, suffix: str) -> str:
        """Build full MQTT topic."""
        if self.device_id:
            return f"{self.topic_prefix}/{self.device_id}/{suffix}"
        return f"{self.topic_prefix}/{suffix}"

    async def async_setup(self) -> None:
        """Set up MQTT subscriptions."""
        # Subscribe to status topic
        self._subscriptions.append(
            await mqtt.async_subscribe(
                self.hass,
                self._build_topic(TOPIC_STATUS),
                self._handle_status_message,
                qos=1,
            )
        )
        
        # Subscribe to power topic
        self._subscriptions.append(
            await mqtt.async_subscribe(
                self.hass,
                self._build_topic(TOPIC_POWER),
                self._handle_power_message,
                qos=1,
            )
        )
        
        # Subscribe to statistics topic
        self._subscriptions.append(
            await mqtt.async_subscribe(
                self.hass,
                self._build_topic(TOPIC_STATISTICS),
                self._handle_statistics_message,
                qos=1,
            )
        )
        
        # Subscribe to availability topic
        self._subscriptions.append(
            await mqtt.async_subscribe(
                self.hass,
                self._build_topic(TOPIC_AVAILABILITY),
                self._handle_availability_message,
                qos=1,
            )
        )
        
        _LOGGER.info("BrewOS MQTT subscriptions set up for prefix: %s", self.topic_prefix)

    async def async_shutdown(self) -> None:
        """Shutdown and unsubscribe from MQTT."""
        for unsubscribe in self._subscriptions:
            unsubscribe()
        self._subscriptions.clear()

    @callback
    def _handle_status_message(self, msg: mqtt.ReceiveMessage) -> None:
        """Handle status MQTT message."""
        try:
            payload = json.loads(msg.payload)
            
            # Update data
            self.data.update({
                "state": payload.get("state", "standby"),
                "mode": payload.get("mode", "standby"),
                "heating_strategy": int(payload.get("heating_strategy", 1)),
                "brew_temp": float(payload.get("brew_temp", 0)),
                "brew_setpoint": float(payload.get("brew_setpoint", 93.5)),
                "steam_temp": float(payload.get("steam_temp", 0)),
                "steam_setpoint": float(payload.get("steam_setpoint", 145.0)),
                "pressure": float(payload.get("pressure", 0)),
                "scale_weight": float(payload.get("scale_weight", 0)),
                "flow_rate": float(payload.get("flow_rate", 0)),
                "shot_duration": float(payload.get("shot_duration", 0)),
                "shot_weight": float(payload.get("shot_weight", 0)),
                "target_weight": float(payload.get("target_weight", 36.0)),
                "is_brewing": payload.get("is_brewing", False),
                "is_heating": payload.get("is_heating", False),
                "water_low": payload.get("water_low", False),
                "alarm_active": payload.get("alarm_active", False),
                "pico_connected": payload.get("pico_connected", False),
                "wifi_connected": payload.get("wifi_connected", True),
                "scale_connected": payload.get("scale_connected", False),
            })
            
            self.async_set_updated_data(self.data)
            
        except (json.JSONDecodeError, ValueError) as err:
            _LOGGER.error("Error parsing status message: %s", err)

    @callback
    def _handle_power_message(self, msg: mqtt.ReceiveMessage) -> None:
        """Handle power meter MQTT message."""
        try:
            payload = json.loads(msg.payload)
            
            self.data.update({
                "power": float(payload.get("power", 0)),
                "voltage": float(payload.get("voltage", 0)),
                "current": float(payload.get("current", 0)),
                "energy_import": float(payload.get("energy_import", 0)),
                "frequency": float(payload.get("frequency", 0)),
                "power_factor": float(payload.get("power_factor", 0)),
            })
            
            self.async_set_updated_data(self.data)
            
        except (json.JSONDecodeError, ValueError) as err:
            _LOGGER.error("Error parsing power message: %s", err)

    @callback
    def _handle_statistics_message(self, msg: mqtt.ReceiveMessage) -> None:
        """Handle statistics MQTT message."""
        try:
            payload = json.loads(msg.payload)
            
            self.data.update({
                "shots_today": int(payload.get("shots_today", 0)),
                "total_shots": int(payload.get("total_shots", 0)),
                "kwh_today": float(payload.get("kwh_today", 0)),
            })
            
            self.async_set_updated_data(self.data)
            
        except (json.JSONDecodeError, ValueError) as err:
            _LOGGER.error("Error parsing statistics message: %s", err)

    @callback
    def _handle_availability_message(self, msg: mqtt.ReceiveMessage) -> None:
        """Handle availability MQTT message."""
        available = msg.payload.decode() == "online"
        self._available = available
        self.data["available"] = available
        self.async_set_updated_data(self.data)

    @property
    def available(self) -> bool:
        """Return if the device is available."""
        return self._available

    async def async_send_command(self, command: str, **kwargs: Any) -> None:
        """Send a command to the device via MQTT."""
        payload = {"cmd": command, **kwargs}
        topic = self._build_topic(TOPIC_COMMAND)
        
        await mqtt.async_publish(
            self.hass,
            topic,
            json.dumps(payload),
            qos=1,
        )
        
        _LOGGER.debug("Sent command %s to %s", command, topic)

