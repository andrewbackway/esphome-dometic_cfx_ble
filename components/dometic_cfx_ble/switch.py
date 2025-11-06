import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch as esphome_switch
from esphome.const import CONF_TYPE

from . import DometicCfxBle, CONF_DOMETIC_CFX_BLE_ID, validate_topic_type

CONFIG_SCHEMA = esphome_switch.switch_schema().extend(
    {
        cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
        cv.Required(CONF_TYPE): validate_topic_type,
    }
)


async def to_code(config):
    var = await esphome_switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))
