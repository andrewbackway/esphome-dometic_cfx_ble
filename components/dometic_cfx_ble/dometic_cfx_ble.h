#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/entity_base.h"

#include "esphome/components/ble_client/ble_client.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"

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

struct TopicInfo {
  uint8_t param[4];
  const char *type;
  const char *description;
};

extern const std::map<std::string, TopicInfo> TOPICS;

class DometicCfxBle : public Component, public ble_client::BLEClientNode {

 public:

  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }



  void set_product_type(uint8_t type) { this->product_type_ = type; }

  void set_temperature_unit(const std::string &unit) { this->temperature_unit_ = unit; }
  const std::string &get_temperature_unit() const { return this->temperature_unit_; }

  void apply_temperature_units() {
    const char *unit_str = (this->temperature_unit_ == "F") ? "°F" : "°C";
    // Apply unit to measured temperature sensors
    if (this->sensors_.count("COMPARTMENT_0_MEASURED_TEMPERATURE")) {
      this->sensors_["COMPARTMENT_0_MEASURED_TEMPERATURE"]->set_unit_of_measurement(unit_str);
    }
    if (this->sensors_.count("COMPARTMENT_1_MEASURED_TEMPERATURE")) {
      this->sensors_["COMPARTMENT_1_MEASURED_TEMPERATURE"]->set_unit_of_measurement(unit_str);
    }
  }



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



  void send_pub(const std::string &topic, const std::vector<uint8_t> &value);

  void send_sub(const std::string &topic);

  void send_ping();



  void send_switch(const std::string &topic, bool value);

  void send_number(const std::string &topic, float value);



  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,

                           esp_ble_gattc_cb_param_t *param) override;



  bool is_connected() const { return connected_; }



 protected:

  void handle_notify_(const uint8_t *data, uint16_t length);

  void update_entity_(const std::string &topic, const std::vector<uint8_t> &value);



  float decode_to_float_(const std::vector<uint8_t> &bytes, const std::string &type_hint);

  bool decode_to_bool_(const std::vector<uint8_t> &bytes, const std::string &type_hint);

  std::string decode_to_string_(const std::vector<uint8_t> &bytes, const std::string &type_hint);



  std::vector<uint8_t> encode_from_bool_(bool value, const std::string &type_hint);

  std::vector<uint8_t> encode_from_float_(float value, const std::string &type_hint);



  std::string get_english_desc_(const std::string &topic_key,

                                const TopicInfo &info,

                                const std::vector<uint8_t> &bytes);



  uint8_t product_type_{0};

  std::string temperature_unit_{"C"};



  uint16_t write_handle_{0};

  uint16_t notify_handle_{0};

  bool connected_{false};

  bool notify_registered_{false};



  uint32_t last_activity_ms_{0};



  std::queue<std::vector<uint8_t>> send_queue_;



  std::map<std::string, sensor::Sensor *> sensors_;

  std::map<std::string, binary_sensor::BinarySensor *> binary_sensors_;

  

  std::map<std::string, switch_::Switch *> switches_;

  std::map<std::string, number::Number *> numbers_;

  std::map<std::string, text_sensor::TextSensor *> text_sensors_;

};

class DometicCfxBleSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleBinarySensor : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleSwitch : public switch_::Switch, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }

  void write_state(bool state) override;
  void update() override {}

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleNumber : public number::Number, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }

  void control(float value) override;
  void update() override {}

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

class DometicCfxBleTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(DometicCfxBle *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void update() override {}

 protected:
  DometicCfxBle *parent_{nullptr};
  std::string topic_;
};

}  // namespace dometic_cfx_ble
}  // namespace esphome
