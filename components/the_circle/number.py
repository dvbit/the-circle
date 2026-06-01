"""
The Circle – Number platform
Registers editing number entities for params, colors, intensity, thresholds.

Ref: ESPHome number platform pattern
  https://esphome.io/components/number/
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID

from . import the_circle_ns, TheCircleComponent

DEPENDENCIES = ["the_circle"]

# C++ class references
ParamNumber = the_circle_ns.class_(
    "ParamNumber", number.Number, cg.Component
)
ColorComponentNumber = the_circle_ns.class_(
    "ColorComponentNumber", number.Number, cg.Component
)
ColorIndexNumber = the_circle_ns.class_(
    "ColorIndexNumber", number.Number, cg.Component
)
IntensityNumber = the_circle_ns.class_(
    "IntensityNumber", number.Number, cg.Component
)
ValueMapNumber = the_circle_ns.class_(
    "ValueMapNumber", number.Number, cg.Component
)
ThresholdNumber = the_circle_ns.class_(
    "ThresholdNumber", number.Number, cg.Component
)
ThresholdEnabledNumber = the_circle_ns.class_(
    "ThresholdEnabledNumber", number.Number, cg.Component
)

# Config keys
CONF_THE_CIRCLE_ID = "the_circle_id"
CONF_PARAM0 = "param0"
CONF_PARAM1 = "param1"
CONF_PARAM2 = "param2"
CONF_PARAM3 = "param3"
CONF_COLOR_R = "color_r"
CONF_COLOR_G = "color_g"
CONF_COLOR_B = "color_b"
CONF_COLOR_INDEX = "color_index"
CONF_INTENSITY = "intensity"
CONF_VALUE_MIN = "value_min"
CONF_VALUE_MAX = "value_max"
CONF_THRESHOLD1 = "threshold1"
CONF_THRESHOLD2 = "threshold2"
CONF_THRESHOLD_ENABLED = "threshold_enabled"

# Schema for param numbers (0–1000 range, box mode)
PARAM_SCHEMA = number.number_schema(ParamNumber).extend(
    cv.COMPONENT_SCHEMA
)

# Schema for color component numbers (0–255)
COLOR_SCHEMA = number.number_schema(ColorComponentNumber).extend(
    cv.COMPONENT_SCHEMA
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THE_CIRCLE_ID): cv.use_id(TheCircleComponent),
        # Params 0–3
        cv.Optional(CONF_PARAM0): PARAM_SCHEMA,
        cv.Optional(CONF_PARAM1): PARAM_SCHEMA,
        cv.Optional(CONF_PARAM2): PARAM_SCHEMA,
        cv.Optional(CONF_PARAM3): PARAM_SCHEMA,
        # Color R/G/B
        cv.Optional(CONF_COLOR_R): COLOR_SCHEMA,
        cv.Optional(CONF_COLOR_G): COLOR_SCHEMA,
        cv.Optional(CONF_COLOR_B): COLOR_SCHEMA,
        # Color index
        cv.Optional(CONF_COLOR_INDEX): number.number_schema(
            ColorIndexNumber
        ).extend(cv.COMPONENT_SCHEMA),
        # Intensity
        cv.Optional(CONF_INTENSITY): number.number_schema(
            IntensityNumber
        ).extend(cv.COMPONENT_SCHEMA),
        # Value mapping
        cv.Optional(CONF_VALUE_MIN): number.number_schema(
            ValueMapNumber
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_VALUE_MAX): number.number_schema(
            ValueMapNumber
        ).extend(cv.COMPONENT_SCHEMA),
        # Threshold
        cv.Optional(CONF_THRESHOLD1): number.number_schema(
            ThresholdNumber
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_THRESHOLD2): number.number_schema(
            ThresholdNumber
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_THRESHOLD_ENABLED): number.number_schema(
            ThresholdEnabledNumber
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_THE_CIRCLE_ID])

    # Param numbers with index
    for key, idx in [
        (CONF_PARAM0, 0),
        (CONF_PARAM1, 1),
        (CONF_PARAM2, 2),
        (CONF_PARAM3, 3),
    ]:
        if key in config:
            var = await number.new_number(
                config[key],
                min_value=0,
                max_value=1000,
                step=1,
            )
            await cg.register_component(var, config[key])
            cg.add(var.set_circle(parent))
            cg.add(var.set_param_index(idx))

    # Color component numbers with channel
    for key, ch in [
        (CONF_COLOR_R, 0),
        (CONF_COLOR_G, 1),
        (CONF_COLOR_B, 2),
    ]:
        if key in config:
            var = await number.new_number(
                config[key],
                min_value=0,
                max_value=255,
                step=1,
            )
            await cg.register_component(var, config[key])
            cg.add(var.set_circle(parent))
            cg.add(var.set_channel(ch))

    # Color index
    if CONF_COLOR_INDEX in config:
        var = await number.new_number(
            config[CONF_COLOR_INDEX],
            min_value=0,
            max_value=3,
            step=1,
        )
        await cg.register_component(var, config[CONF_COLOR_INDEX])
        cg.add(var.set_circle(parent))

    # Intensity
    if CONF_INTENSITY in config:
        var = await number.new_number(
            config[CONF_INTENSITY],
            min_value=0,
            max_value=255,
            step=1,
        )
        await cg.register_component(var, config[CONF_INTENSITY])
        cg.add(var.set_circle(parent))

    # Value map min/max
    for key, is_max in [
        (CONF_VALUE_MIN, False),
        (CONF_VALUE_MAX, True),
    ]:
        if key in config:
            var = await number.new_number(
                config[key],
                min_value=-10000,
                max_value=10000,
                step=0.1,
            )
            await cg.register_component(var, config[key])
            cg.add(var.set_circle(parent))
            cg.add(var.set_is_max(is_max))

    # Threshold numbers
    for key, idx in [
        (CONF_THRESHOLD1, 0),
        (CONF_THRESHOLD2, 1),
    ]:
        if key in config:
            var = await number.new_number(
                config[key],
                min_value=0,
                max_value=10000,
                step=0.1,
            )
            await cg.register_component(var, config[key])
            cg.add(var.set_circle(parent))
            cg.add(var.set_threshold_index(idx))

    # Threshold enabled
    if CONF_THRESHOLD_ENABLED in config:
        var = await number.new_number(
            config[CONF_THRESHOLD_ENABLED],
            min_value=0,
            max_value=1,
            step=1,
        )
        await cg.register_component(var, config[CONF_THRESHOLD_ENABLED])
        cg.add(var.set_circle(parent))
