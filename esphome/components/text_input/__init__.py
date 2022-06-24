from typing import Text
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
#    CONF_OPERATION, 
    CONF_VALUE,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

text_input_ns = cg.esphome_ns.namespace("text_input")
TextInput = text_input_ns.class_("TextInput", cg.EntityBase)
TextInputPtr = TextInput.operator("ptr")

# Triggers
TextInputStateTrigger = text_input_ns.class_(
    "TextInputStateTrigger", automation.Trigger.template(cg.float_)
)
#ValueRangeTrigger = text_input_ns.class_(
#    "ValueRangeTrigger", automation.Trigger.template(cg.float_), cg.Component
#)

# Actions
TextInputSetAction = text_input_ns.class_("TextInputSetAction", automation.Action)
#TextInputOperationAction = text_input_ns.class_("TextInputOperationAction", automation.Action)

TextInputMode = text_input_ns.enum("TextInputMode")

TEXT_INPUT_MODES = {
    "AUTO": TextInputMode.TEXT_INPUT_MODE_AUTO,
    "STRING": TextInputMode.TEXT_INPUT_MODE_BOX,
#    "SECRET": TextInputMode.TEXT_INPUT_MODE_SECRET,  # to be implemented for keys, passwords, etc.
}

#TextInputOperation = text_input_ns.enum("TextInputOperation")

#TEXT_INPUT_OPERATION_OPTIONS = {
#    "XXX": TextInputOperation.TEXT_INPUT_OP_XXX,
#    "YYY": TextInputOperation.TEXT_INPUT_OP_YYY,
#}

icon = cv.icon


TEXT_INPUT_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend( # of MQTT_COMMAND_COMPONENT_SCHEMA
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTTextInput),
        cv.GenerateID(): cv.declare_id(TextInput),
 #       cv.Optional(CONF_FILTERS): validate_filters,
 #       cv.Optional(CONF_ON_VALUE): automation.validate_automation(
 #           {
 #               cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TextInputStateTrigger),
 #           }
 #       ),
#        cv.Optional(CONF_ON_RAW_VALUE): automation.validate_automation(
#            {
#                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
#                    TextSensorStateRawTrigger
#                ),
#            }
#        ),
        cv.Optional(CONF_MODE, default="AUTO"): cv.enum(TEXT_INPUT_MODES, upper=True),
    }
)


async def setup_text_input_core_(
    var, config
):
    await setup_entity(var, config)

    cg.add(var.traits.set_mode(config[CONF_MODE]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_text_input(
    var, config
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text_input(var))
    await setup_text_input_core_(
        var, config
    )


async def new_text_input(
    config
):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_text_input(
        var, config
    )
    return var



async def text_input_state_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)
#    templ = await cg.templatable(config[CONF_STATE], args, cg.std_string)
#    cg.add(var.set_state(templ))
    return var
