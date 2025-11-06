# number.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_NAME, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP, CONF_UNIT_OF_MEASUREMENT
from esphome.components import number as esphome_number

from . import dometic_cfx_ble_ns, DometicCfxBle, CONF_DOMETIC_CFX_BLE_ID, TOPIC_TYPES, validate_topic_type

DometicCfxBleNumber = dometic_cfx_ble_ns.class_("DometicCfxBleNumber", esphome_number.Number, cg.PollingComponent)

CONFIG_SCHEMA = esphome_number.number_schema(DometicCfxBleNumber).extend({
    cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
    cv.Required(CONF_TYPE): validate_topic_type,
}).extend(cv.polling_component_schema('60s'))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esphome_number.register_number(var, config)
    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))
    cg.add(var.set_parent(parent))
    cg.add(var.set_topic(config[CONF_TYPE]))

