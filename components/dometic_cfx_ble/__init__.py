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

dometic_cfx_ble_ns = cg.esphome_ns.namespace("dometic_cfx_ble")
DometicCfxBle = dometic_cfx_ble_ns.class_("DometicCfxBle", cg.Component)

CONF_PRODUCT_TYPE = "product_type"
CONF_DOMETIC_CFX_BLE_ID = "dometic_cfx_ble_id"

PRODUCT_TYPES = cv.enum(
    {
        "SZ": 1,
        "SZI": 2,
        "DZ": 3,
    },
    upper=True,
)

TOPIC_TYPES = cv.one_of(
    "SUBSCRIBE_APP_SZ",
    "SUBSCRIBE_APP_SZI",
    "SUBSCRIBE_APP_DZ",
    "PRODUCT_SERIAL_NUMBER",
    "COMPARTMENT_COUNT",
    "ICEMAKER_COUNT",
    "COMPARTMENT_0_POWER",
    "COMPARTMENT_1_POWER",
    "COMPARTMENT_0_MEASURED_TEMPERATURE",
    "COMPARTMENT_1_MEASURED_TEMPERATURE",
    "COMPARTMENT_0_DOOR_OPEN",
    "COMPARTMENT_1_DOOR_OPEN",
    "COMPARTMENT_0_SET_TEMPERATURE",
    "COMPARTMENT_1_SET_TEMPERATURE",
    "COMPARTMENT_0_RECOMMENDED_RANGE",
    "COMPARTMENT_1_RECOMMENDED_RANGE",
    "PRESENTED_TEMPERATURE_UNIT",
    "COMPARTMENT_0_TEMPERATURE_RANGE",
    "COMPARTMENT_1_TEMPERATURE_RANGE",
    "COOLER_POWER",
    "BATTERY_VOLTAGE_LEVEL",
    "BATTERY_PROTECTION_LEVEL",
    "POWER_SOURCE",
    "ICEMAKER_POWER",
    "COMMUNICATION_ALARM",
    "NTC_OPEN_LARGE_ERROR",
    "NTC_SHORT_LARGE_ERROR",
    "SOLENOID_VALVE_ERROR",
    "NTC_OPEN_SMALL_ERROR",
    "NTC_SHORT_SMALL_ERROR",
    "FAN_OVERVOLTAGE_ERROR",
    "COMPRESSOR_START_FAIL_ERROR",
    "COMPRESSOR_SPEED_ERROR",
    "CONTROLLER_OVER_TEMPERATURE",
    "TEMPERATURE_ALERT_DCM",
    "TEMPERATURE_ALERT_CC",
    "DOOR_ALERT",
    "VOLTAGE_ALERT",
    "DEVICE_NAME",
    "WIFI_MODE",
    "BLUETOOTH_MODE",
    "WIFI_AP_CONNECTED",
    "STATION_SSID_0",
    "STATION_SSID_1",
    "STATION_SSID_2",
    "STATION_PASSWORD_0",
    "STATION_PASSWORD_1",
    "STATION_PASSWORD_2",
    "STATION_PASSWORD_3",
    "STATION_PASSWORD_4",
    "CFX_DIRECT_PASSWORD_0",
    "CFX_DIRECT_PASSWORD_1",
    "CFX_DIRECT_PASSWORD_2",
    "CFX_DIRECT_PASSWORD_3",
    "CFX_DIRECT_PASSWORD_4",
    "COMPARTMENT_0_TEMPERATURE_HISTORY_HOUR",
    "COMPARTMENT_1_TEMPERATURE_HISTORY_HOUR",
    "COMPARTMENT_0_TEMPERATURE_HISTORY_DAY",
    "COMPARTMENT_1_TEMPERATURE_HISTORY_DAY",
    "COMPARTMENT_0_TEMPERATURE_HISTORY_WEEK",
    "COMPARTMENT_1_TEMPERATURE_HISTORY_WEEK",
    "DC_CURRENT_HISTORY_HOUR",
    "DC_CURRENT_HISTORY_DAY",
    "DC_CURRENT_HISTORY_WEEK",
)

CONFIG_SCHEMA = cv.Schema(
    {
        # use cv.declare_id, not cg.declare_id
        cv.GenerateID(CONF_ID): cv.declare_id(DometicCfxBle),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Required(CONF_PRODUCT_TYPE): PRODUCT_TYPES,
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
    # MACAddress has .as_hex
    cg.add(var.set_mac_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_product_type(config[CONF_PRODUCT_TYPE]))

