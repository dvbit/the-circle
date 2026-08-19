"""
The Circle – Text Sensor platform
Registers JSON export sensors for card integration.

- profiles_list: overview of all profiles (names, active, LED counts)
- profile_config: full config of a requested profile (on-demand via service)

Ref: ESPHome text_sensor platform pattern
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import the_circle_ns, TheCircleComponent

DEPENDENCIES = ["the_circle"]

# Config keys
CONF_THE_CIRCLE_ID = "the_circle_id"
CONF_PROFILES_LIST = "profiles_list"
CONF_PROFILE_CONFIG = "profile_config"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THE_CIRCLE_ID): cv.use_id(TheCircleComponent),
        cv.Optional(CONF_PROFILES_LIST): text_sensor.text_sensor_schema().extend(
            cv.COMPONENT_SCHEMA
        ),
        cv.Optional(CONF_PROFILE_CONFIG): text_sensor.text_sensor_schema().extend(
            cv.COMPONENT_SCHEMA
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_THE_CIRCLE_ID])

    if CONF_PROFILES_LIST in config:
        var = await text_sensor.new_text_sensor(config[CONF_PROFILES_LIST])
        cg.add(parent.set_profiles_list_sensor(var))

    if CONF_PROFILE_CONFIG in config:
        var = await text_sensor.new_text_sensor(config[CONF_PROFILE_CONFIG])
        cg.add(parent.set_profile_config_sensor(var))
