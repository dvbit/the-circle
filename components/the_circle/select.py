"""
The Circle – Select platform
Registers navigation and editing select entities.

Ref: ESPHome select platform pattern
  https://esphome.io/components/select/
Ref: ESPHome 2025.11+ – options must be passed via Python codegen
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from . import the_circle_ns, TheCircleComponent

DEPENDENCIES = ["the_circle"]

# C++ class references
EditProfileSelect = the_circle_ns.class_(
    "EditProfileSelect", select.Select, cg.Component
)
EditStripSelect = the_circle_ns.class_(
    "EditStripSelect", select.Select, cg.Component
)
EditLayerSelect = the_circle_ns.class_(
    "EditLayerSelect", select.Select, cg.Component
)
PrimitiveTypeSelect = the_circle_ns.class_(
    "PrimitiveTypeSelect", select.Select, cg.Component
)
ActiveProfileSelect = the_circle_ns.class_(
    "ActiveProfileSelect", select.Select, cg.Component
)

# Config keys
CONF_THE_CIRCLE_ID = "the_circle_id"
CONF_EDIT_PROFILE = "edit_profile"
CONF_EDIT_STRIP = "edit_strip"
CONF_EDIT_LAYER = "edit_layer"
CONF_PRIMITIVE_TYPE = "primitive_type"
CONF_ACTIVE_PROFILE = "active_profile"

# Fixed option lists (set at compile time, stored in flash)
PROFILE_OPTIONS = [f"Profile {i+1}" for i in range(10)]
STRIP_OPTIONS = ["Inner Aura", "Outer Aura", "Inner Glow"]
LAYER_OPTIONS = [f"Layer {i+1}" for i in range(6)]
PRIMITIVE_TYPE_OPTIONS = [
    "None", "Dot", "Arc", "Trail", "Solid",
    "Gradient", "Segment", "Pulse", "Spin",
    "Rainbow", "Strobe", "Sparkle", "Comet", "Threshold",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THE_CIRCLE_ID): cv.use_id(TheCircleComponent),
        cv.Optional(CONF_EDIT_PROFILE): select.select_schema(
            EditProfileSelect
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_EDIT_STRIP): select.select_schema(
            EditStripSelect
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_EDIT_LAYER): select.select_schema(
            EditLayerSelect
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_PRIMITIVE_TYPE): select.select_schema(
            PrimitiveTypeSelect
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_ACTIVE_PROFILE): select.select_schema(
            ActiveProfileSelect
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_THE_CIRCLE_ID])

    # Each select gets its fixed options list via codegen
    for key, cls, opts in [
        (CONF_EDIT_PROFILE, EditProfileSelect, PROFILE_OPTIONS),
        (CONF_EDIT_STRIP, EditStripSelect, STRIP_OPTIONS),
        (CONF_EDIT_LAYER, EditLayerSelect, LAYER_OPTIONS),
        (CONF_PRIMITIVE_TYPE, PrimitiveTypeSelect, PRIMITIVE_TYPE_OPTIONS),
        (CONF_ACTIVE_PROFILE, ActiveProfileSelect, PROFILE_OPTIONS),
    ]:
        if key in config:
            conf = config[key]
            var = await select.new_select(conf, options=opts)
            await cg.register_component(var, conf)
            cg.add(var.set_circle(parent))
