import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor as esphome_sensor
from esphome.const import CONF_TYPE

from . import DometicCfxBle, CONF_DOMETIC_CFX_BLE_ID, validate_topic_type

CONFIG_SCHEMA = esphome_sensor.sensor_schema().extend(
    {
        cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
        cv.Required(CONF_TYPE): validate_topic_type,
    }
)


async def to_code(config):
    # Create the sensor using the modern helper
    var = await esphome_sensor.new_sensor(config)

    # Link to the Dometic hub and register the topic ↔ entity mapping
    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))
