# text_sensor.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_NAME
from esphome.components import text_sensor as esphome_text_sensor

from . import dometic_cfx_ble_ns, DometicCfxBle, CONF_DOMETIC_CFX_BLE_ID, validate_topic_type

DometicCfxBleTextSensor = dometic_cfx_ble_ns.class_("DometicCfxBleTextSensor", esphome_text_sensor.TextSensor, cg.PollingComponent)

CONFIG_SCHEMA = esphome_text_sensor.text_sensor_schema(DometicCfxBleTextSensor).extend({
    cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
    cv.Required(CONF_TYPE): validate_topic_type,
}).extend(cv.polling_component_schema('60s'))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esphome_text_sensor.register_text_sensor(var, config)
    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))