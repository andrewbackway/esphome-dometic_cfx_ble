import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_TYPE,
    CONF_NAME,
    CONF_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
)
from esphome.cpp_types import Component
from esphome import automation
from esphome.components.ble_client import CONF_BLE_CLIENT_ID
from esphome.components import ble_client

AUTO_LOAD = ["esp32_ble_tracker", "ble_client", "select", "number", "switch", "text_sensor", "binary_sensor", "sensor"]
DEPENDENCIES = ['esp32_ble_tracker', 'ble_client']


dometic_cfx_ble_ns = cg.esphome_ns.namespace("dometic_cfx_ble")
DometicCfxBle = dometic_cfx_ble_ns.class_("DometicCfxBle", cg.Component, ble_client.BLEClientNode)

CONF_PRODUCT_TYPE = "product_type"
CONF_DOMETIC_CFX_BLE_ID = "dometic_cfx_ble_id"
CONF_TEMPERATURE_UNIT = "temperature_unit"

# NOTE: under the CFX5 protocol the component subscribes to every known
# parameter individually (see ESP_GATTC_REG_FOR_NOTIFY_EVT in the .cpp), so
# product_type no longer selects a "subscribe all" command like it did for
# CFX3. Kept here only so existing YAML configs with `product_type:` don't
# break; the value itself is currently unused.
PRODUCT_TYPES = cv.enum(
    {
        "SZ": 1,
        "SZI": 2,
        "DZ": 3,
    },
    upper=True,
)

# CFX5 "MC1" generation topics - must stay in sync with the TOPICS map in
# dometic_cfx_ble.cpp. The CFX3 topic list (serial number, wifi config,
# history arrays, etc.) does not apply here - none of that has been
# reverse-engineered for the CFX5 yet.
TOPIC_TYPES = [
    "COMPARTMENT_0_MEASURED_TEMPERATURE",
    "COMPARTMENT_1_MEASURED_TEMPERATURE",
    "COMPARTMENT_0_SET_TEMPERATURE",
    "COMPARTMENT_1_SET_TEMPERATURE",
    "COMPARTMENT_0_COMPRESSOR",
    "COMPARTMENT_1_COMPRESSOR",
    "COMPARTMENT_0_DOOR_OPEN",
    "COMPARTMENT_1_DOOR_OPEN",
    "COMPARTMENT_POWER",
    "DC_VOLTAGE",
    "POWER_SOURCE",
    "DOOR_ALERT",
]

# Topics whose values are temperatures - used by sensor.py and number.py to
# auto-set unit_of_measurement.
TEMPERATURE_TOPICS = {
    "COMPARTMENT_0_MEASURED_TEMPERATURE",
    "COMPARTMENT_1_MEASURED_TEMPERATURE",
    "COMPARTMENT_0_SET_TEMPERATURE",
    "COMPARTMENT_1_SET_TEMPERATURE",
}

# Registry populated during to_code so that platform modules (sensor, number)
# can look up the parent component's configuration at code-generation time.
_DOMETIC_CONFIGS = {}  # str(core.ID) -> config dict


def validate_topic_type(value):
    """Ensure the YAML 'type' is one of the known topic types."""
    value = cv.string_strict(value)
    if value not in TOPIC_TYPES:
        raise cv.Invalid(
            f"Invalid dometic_cfx_ble type '{value}'. "
            f"Valid values: {', '.join(TOPIC_TYPES)}"
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        # use cv.declare_id, not cg.declare_id
        cv.GenerateID(CONF_ID): cv.declare_id(DometicCfxBle),
        cv.Required(CONF_BLE_CLIENT_ID): cv.use_id(type("esphome.components.ble_client.ble_client.BLEClient")),
        cv.Required(CONF_PRODUCT_TYPE): PRODUCT_TYPES,
        cv.Optional(CONF_TEMPERATURE_UNIT, default="C"): cv.enum({"C": "C", "F": "F"}, upper=True),
    }
).extend(cv.COMPONENT_SCHEMA)


def entity_schema(platform):
    base = {
        cv.GenerateID(): cv.declare_id(
            cg.esphome_ns.class_(f"DometicCfxBle{platform.capitalize()}")
        ),
        cv.Required(CONF_DOMETIC_CFX_BLE_ID): cv.use_id(DometicCfxBle),
        cv.Required(CONF_TYPE): TOPIC_TYPES,
        cv.Required(CONF_NAME): cv.string,
    }
    if platform == "sensor":
        base.update(
            {
                CONF_UNIT_OF_MEASUREMENT: cv.string,
                CONF_ACCURACY_DECIMALS: cv.int_,
            }
        )
    if platform == "number":
        base.update(
            {
                CONF_MIN_VALUE: cv.float_,
                CONF_MAX_VALUE: cv.float_,
                CONF_STEP: cv.float_,
                CONF_UNIT_OF_MEASUREMENT: cv.string,
            }
        )
    return cv.Schema(base).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    ble_client_var = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(ble_client_var.register_ble_node(var))
    cg.add(var.set_product_type(config[CONF_PRODUCT_TYPE]))
    cg.add(var.set_temperature_unit(config[CONF_TEMPERATURE_UNIT]))
    _DOMETIC_CONFIGS[str(config[CONF_ID])] = config

