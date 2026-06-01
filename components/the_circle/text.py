"""
The Circle – Text platform
Registers entity_id and profile_name text entities.

Ref: ESPHome text platform pattern
  https://esphome.io/components/text/
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text
from esphome.const import CONF_ID

from . import the_circle_ns, TheCircleComponent

DEPENDENCIES = ["the_circle"]

# C++ class references
EntityIdText = the_circle_ns.class_(
    "EntityIdText", text.Text, cg.Component
)
ProfileNameText = the_circle_ns.class_(
    "ProfileNameText", text.Text, cg.Component
)

# Config keys
CONF_THE_CIRCLE_ID = "the_circle_id"
CONF_ENTITY_ID_INPUT = "entity_id_input"
CONF_PROFILE_NAME = "profile_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THE_CIRCLE_ID): cv.use_id(TheCircleComponent),
        cv.Optional(CONF_ENTITY_ID_INPUT): text.text_schema(
            EntityIdText
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_PROFILE_NAME): text.text_schema(
            ProfileNameText
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_THE_CIRCLE_ID])

    if CONF_ENTITY_ID_INPUT in config:
        var = await text.new_text(config[CONF_ENTITY_ID_INPUT])
        await cg.register_component(var, config[CONF_ENTITY_ID_INPUT])
        cg.add(var.set_circle(parent))

    if CONF_PROFILE_NAME in config:
        var = await text.new_text(
            config[CONF_PROFILE_NAME],
            min_length=0,
            max_length=15,
        )
        await cg.register_component(var, config[CONF_PROFILE_NAME])
        cg.add(var.set_circle(parent))
