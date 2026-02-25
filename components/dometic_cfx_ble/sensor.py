import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor as esphome_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS
)

from . import (
    dometic_cfx_ble_ns,
    DometicCfxBle,
    CONF_DOMETIC_CFX_BLE_ID,
    validate_topic_type,
    CONF_TEMPERATURE_UNIT
)


DometicCfxBleSensor = dometic_cfx_ble_ns.class_("DometicCfxBleSensor", esphome_sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = esphome_sensor.sensor_schema(
    unit_of_measurement=CONF_UNIT_OF_MEASUREMENT,
    accuracy_decimals=1
).extend({
    cv.GenerateID(): cv.declare_id(DometicCfxBleSensor),
    cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
    cv.Required(CONF_TYPE): validate_topic_type,
}).extend(cv.polling_component_schema('60s'))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esphome_sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DOMETIC_CFX_BLE_ID])
    cg.add(parent.add_entity(config[CONF_TYPE], var))