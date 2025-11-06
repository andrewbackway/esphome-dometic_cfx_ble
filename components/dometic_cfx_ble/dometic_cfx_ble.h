#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/entity_base.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_main.h>
#include <esp_gatt_defs.h>

#include <cstdint>
#include <cstring>
#include <queue>
#include <map>
#include <string>
#include <vector>
#include <type_traits>
#include <cmath>

namespace esphome {
namespace dometic_cfx_ble {

static const char *const TAG = "dometic_cfx_ble";

// Sentinel for "no value" (INT16_DECIDEGREE_CELSIUS -32768 / 10)
static constexpr float NO_VALUE = -3276.8f;

// Topic description from the DDM mapping
struct TopicInfo {
  uint8_t param[4];       // 4-byte key
  const char *type;       // e.g. "INT16_DECIDEGREE_CELSIUS"
  const char *description;
};

// DDM action codes (exactly as in test.py)
enum : uint8_t {
  ACTION_PUB   = 0,
  ACTION_SUB   = 1,
  ACTION_PING  = 2,
  ACTION_HELLO = 3,
  ACTION_ACK   = 4,
  ACTION_NAK   = 5,
  ACTION_NOP   = 6,
};

// Full topic table is defined in the .cpp
extern const std::map<std::string, TopicInfo> TOPICS;

class DometicCfxBle : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  void set_mac_address(const uint8_t *mac);
  void set_product_type(uint8_t type) { this->product_type_ = type; }

  template<typename T>
  void add_entity(const std::string &topic, T *entity) {
    if constexpr (std::is_base_of<sensor::Sensor, T>::value) {
      sensors_[topic] = entity;
    } else if constexpr (std::is_base_of<binary_sensor::BinarySensor, T>::value) {
      binary_sensors_[topic] = entity;
    } else if constexpr (std::is_base_of<switch_::Switch, T>::value) {
      switches_[topic] = entity;
    } else if constexpr (std::is_base_of<number::Number, T>::value) {
      numbers_[topic] = entity;
    } else if constexpr (std::is_base_of<text_sensor::TextSensor, T>::value) {
      text_sensors_[topic] = entity;
    } else {
      ESP_LOGW(TAG, "Unknown entity type for topic %s", topic.c_str());
    }
  }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // Frame-level helpers (wire format)
  void send_pub(const std::string &topic, const std::vector<uint8_t> &value);
  void send_sub(const std::string &topic);
  void send_ping();

  // Entity helpers so they don't poke internal helpers directly
  void send_switch(const std::string &topic, bool value);
  void send_number(const std::string &topic, float value);

  // Static callback entrypoints
  static DometicCfxBle *instance_;

  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static void gattc_event_handler(esp_gattc_cb_event_t event,
                                  esp_gatt_if_t gatt_if,
                                  esp_ble_gattc_cb_param_t *param);

  // Instance handlers
  void handle_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void handle_gattc_event(esp_gattc_cb_event_t event,
                          esp_gatt_if_t gatt_if,
                          esp_ble_gattc_cb_param_t *param);

  bool is_connected() const { return connected_; }

 protected:
  void start_scan_();
  void connect_();
  void handle_notify_(const uint8_t *data, size_t len);
  void update_entity_(const std::string &topic, const std::vector<uint8_t> &value);

  // Typed decode helpers mirroring test.py
  float decode_to_float_(const std::vector<uint8_t> &bytes, const std::string &type_hint);
  bool decode_to_bool_(const std::vector<uint8_t> &bytes, const std::string &type_hint);
  std::string decode_to_string_(const std::vector<uint8_t> &bytes, const std::string &type_hint);

  std::vector<uint8_t> encode_from_bool_(bool value, const std::string &type_hint);
  std::vector<uint8_t> encode_from_float_(float value, const std::string &type_hint);

  std::string get_english_desc_(const std::string &topic_key,
                                const TopicInfo &info,
                                const std::vector<uint8_t> &bytes);

  uint8_t mac_address_[6] = {0};
  uint8_t product_type_{0};

  esp_gatt_if_t gattc_if_{ESP_GATT_IF_NONE};
  uint16_t conn_id_{0};
  bool connected_{false};

  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};

  uint32_t last_activity_ms_{0};
  bool scan_in_progress_{false};

  std::queue<std::vector<uint8_t>> send_queue_;

  std::map<std::string, sensor::Sensor *> sensors_;
  std::map<std::string, binary_sensor::BinarySensor *> binary_sensors_;
  std::map<std::string, switch_::Switch *> switches_;
  std::map<std::string, number::Number *> numbers_;
  std::map<std::string, text_sensor::TextSensor *> text_sensors_;
};

// Thin wrappers around the hub; they just call back in.

class DometicCfxBleSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}  // notifications only

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleBinarySensor : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}  // notifications only

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleSwitch : public switch_::Switch, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }

  void write_state(bool state) override;
  void update() override {}  // no polling

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleNumber : public number::Number, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }

  void control(float value) override;
  void update() override {}  // no polling

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}  // notifications only

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

}  // namespace dometic_cfx_ble
}  // namespace esphome
