"""
The Circle – ESPHome external component
========================================
Composable LED-ring notification system with HA entity binding.

Architecture:
  - 3 concentric circular LED strips (Inner Aura, Outer Aura, Inner Glow)
  - Up to 6 profiles, each with up to 6 layers (primitives) per strip
  - Painter's algorithm compositing (last layer wins)
  - Runtime HA entity binding via CustomAPIDevice

Ref: ESPHome external_components docs
  https://esphome.io/components/external_components.html
  https://developers.esphome.io/contributing/code/
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.light import LightState

# ---------------------------------------------------------------------------
# Namespace & class registration
# ---------------------------------------------------------------------------
the_circle_ns = cg.esphome_ns.namespace("the_circle")
TheCircleComponent = the_circle_ns.class_(
    "TheCircleComponent", cg.Component
)

# Ensure light component is loaded before us
DEPENDENCIES = ["light", "api"]

# ---------------------------------------------------------------------------
# Config keys
# ---------------------------------------------------------------------------
CONF_STRIPS = "strips"
CONF_LIGHT_ID = "light_id"
CONF_NUM_PROFILES = "num_profiles"
CONF_LAYERS_PER_STRIP = "layers_per_strip"

# ---------------------------------------------------------------------------
# Config schema
# Ref: ESPHome config_validation patterns
# Ref: cv.use_id(LightState) as used in esphome/components/light/automation.py
# ---------------------------------------------------------------------------
STRIP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LIGHT_ID): cv.use_id(LightState),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TheCircleComponent),
        cv.Required(CONF_STRIPS): cv.All(
            cv.ensure_list(STRIP_SCHEMA),
            cv.Length(min=1, max=3),
        ),
        cv.Optional(CONF_NUM_PROFILES, default=20): cv.int_range(min=1, max=20),
        cv.Optional(CONF_LAYERS_PER_STRIP, default=6): cv.int_range(min=1, max=8),
    }
).extend(cv.COMPONENT_SCHEMA)


# ---------------------------------------------------------------------------
# Code generation
# Ref: ESPHome codegen patterns – to_code is async since ESPHome 2023.x
# ---------------------------------------------------------------------------
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Pass strip light IDs to C++ via set_strip()
    for i, strip_conf in enumerate(config[CONF_STRIPS]):
        light = await cg.get_variable(strip_conf[CONF_LIGHT_ID])
        cg.add(var.set_strip(i, light))

    # Pass profile/layer counts
    cg.add(var.set_num_profiles(config[CONF_NUM_PROFILES]))
    cg.add(var.set_layers_per_strip(config[CONF_LAYERS_PER_STRIP]))
