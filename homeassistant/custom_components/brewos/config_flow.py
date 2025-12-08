"""Config flow for BrewOS integration."""
from __future__ import annotations

import logging
from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.core import HomeAssistant
from homeassistant.data_entry_flow import FlowResult
from homeassistant.helpers import selector

from .const import DOMAIN, CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX

_LOGGER = logging.getLogger(__name__)


class BrewOSConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for BrewOS."""

    VERSION = 1

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """Handle the initial step."""
        errors: dict[str, str] = {}

        if user_input is not None:
            # Validate the topic prefix exists in MQTT
            topic_prefix = user_input.get(CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX)
            device_id = user_input.get("device_id", "")
            
            # Create unique ID
            unique_id = f"{topic_prefix}_{device_id}" if device_id else topic_prefix
            await self.async_set_unique_id(unique_id)
            self._abort_if_unique_id_configured()

            return self.async_create_entry(
                title=user_input.get("name", "BrewOS Espresso Machine"),
                data=user_input,
            )

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema(
                {
                    vol.Required("name", default="BrewOS Espresso Machine"): str,
                    vol.Required(CONF_TOPIC_PREFIX, default=DEFAULT_TOPIC_PREFIX): str,
                    vol.Optional("device_id", default=""): str,
                }
            ),
            description_placeholders={
                "topic_info": "Enter the MQTT topic prefix configured in your BrewOS device"
            },
            errors=errors,
        )

    async def async_step_mqtt(self, discovery_info: dict[str, Any]) -> FlowResult:
        """Handle MQTT discovery."""
        # Extract device info from discovery
        topic = discovery_info.get("topic", "")
        
        # Parse topic to get prefix and device_id
        # Expected format: homeassistant/sensor/brewos_DEVICEID/...
        parts = topic.split("/")
        if len(parts) >= 3:
            entity_id = parts[2]  # brewos_DEVICEID
            if entity_id.startswith("brewos_"):
                device_id = entity_id[7:]  # Remove "brewos_" prefix
                
                await self.async_set_unique_id(f"brewos_{device_id}")
                self._abort_if_unique_id_configured()
                
                self.context["title_placeholders"] = {
                    "name": f"BrewOS ({device_id[:8]}...)"
                }
                
                return await self.async_step_mqtt_confirm(device_id=device_id)
        
        return self.async_abort(reason="invalid_discovery_info")

    async def async_step_mqtt_confirm(
        self, user_input: dict[str, Any] | None = None, device_id: str = ""
    ) -> FlowResult:
        """Confirm MQTT discovery."""
        if user_input is not None:
            return self.async_create_entry(
                title=user_input.get("name", f"BrewOS ({device_id[:8]})"),
                data={
                    CONF_TOPIC_PREFIX: DEFAULT_TOPIC_PREFIX,
                    "device_id": device_id,
                    "name": user_input.get("name"),
                },
            )

        return self.async_show_form(
            step_id="mqtt_confirm",
            data_schema=vol.Schema(
                {
                    vol.Required("name", default=f"BrewOS ({device_id[:8]})"): str,
                }
            ),
            description_placeholders={
                "device_id": device_id,
            },
        )

