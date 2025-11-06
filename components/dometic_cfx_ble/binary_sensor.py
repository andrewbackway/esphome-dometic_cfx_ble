import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor as esphome_binary_sensor
from esphome.const import CONF_TYPE

from . import DometicCfxBle, CONF_DOMETIC_CFX_BLE_ID, validate_topic_type

CONFIG_SCHEMA = esphome_binary_sensor.binary_sensor_schema().extend(
    {
        cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
        cv.Required(CONF_TYPE): validate_topic_type,
    }
)


async def to_code(config):
    var = await esphome_binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))
