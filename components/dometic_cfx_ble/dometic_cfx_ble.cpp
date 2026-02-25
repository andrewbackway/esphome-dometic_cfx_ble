#include "dometic_cfx_ble.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

extern "C" {
#include "esp_gattc_api.h"
}

namespace esphome {
namespace dometic_cfx_ble {

// UUID strings (from Dometic app)
static const char *SERVICE_UUID = "537a0300-0995-481f-926c-1604e23fd515";
static const char *WRITE_UUID   = "537a0301-0995-481f-926c-1604e23fd515";
static const char *NOTIFY_UUID  = "537a0302-0995-481f-926c-1604e23fd515";

static const float NO_VALUE = -3276.8f;

static const char *battery_level_str(int v) {
  switch (v) {
    case 0: return "Low";
    case 1: return "Medium";
    case 2: return "High";
    default: return nullptr;
  }
}

static const char *power_source_str(int v) {
  switch (v) {
    case 0: return "AC";
    case 1: return "DC";
    case 2: return "Solar";
    default: return nullptr;
  }
}

// ----------------- Topic table ----------------------------------------------

const std::map<std::string, TopicInfo> TOPICS = {
    {"SUBSCRIBE_APP_SZ", {{1, 0, 0, 129}, "EMPTY", "Subscribe all SZ"}},
    {"SUBSCRIBE_APP_SZI", {{2, 0, 0, 129}, "EMPTY", "Subscribe all SZI"}},
    {"SUBSCRIBE_APP_DZ", {{3, 0, 0, 129}, "EMPTY", "Subscribe all DZ"}},
    {"PRODUCT_SERIAL_NUMBER", {{0, 193, 0, 0}, "UTF8_STRING", "Serial number"}},
    {"COMPARTMENT_COUNT", {{0, 128, 0, 1}, "INT8_NUMBER", "Compartments count"}},
    {"ICEMAKER_COUNT", {{0, 129, 0, 1}, "INT8_NUMBER", "Icemakers count"}},
    {"COMPARTMENT_0_POWER", {{0, 0, 1, 1}, "INT8_BOOLEAN", "Compartment 1 power"}},
    {"COMPARTMENT_1_POWER", {{16, 0, 1, 1}, "INT8_BOOLEAN", "Compartment 2 power"}},
    {"COMPARTMENT_0_MEASURED_TEMPERATURE", {{0, 1, 1, 1}, "INT16_DECIDEGREE_CELSIUS", "Compartment 1 current temp"}},
    {"COMPARTMENT_1_MEASURED_TEMPERATURE", {{16, 1, 1, 1}, "INT16_DECIDEGREE_CELSIUS", "Compartment 2 current temp"}},
    {"COMPARTMENT_0_DOOR_OPEN", {{0, 8, 1, 1}, "INT8_BOOLEAN", "Compartment 1 door open"}},
    {"COMPARTMENT_1_DOOR_OPEN", {{16, 8, 1, 1}, "INT8_BOOLEAN", "Compartment 2 door open"}},
    {"COMPARTMENT_0_SET_TEMPERATURE", {{0, 2, 1, 1}, "INT16_DECIDEGREE_CELSIUS", "Compartment 1 set temp"}},
    {"COMPARTMENT_1_SET_TEMPERATURE", {{16, 2, 1, 1}, "INT16_DECIDEGREE_CELSIUS", "Compartment 2 set temp"}},
    {"COMPARTMENT_0_RECOMMENDED_RANGE", {{0, 129, 1, 1}, "INT16_ARRAY", "Compartment 1 recommended range"}},
    {"COMPARTMENT_1_RECOMMENDED_RANGE", {{16, 129, 1, 1}, "INT16_ARRAY", "Compartment 2 recommended range"}},
    {"PRESENTED_TEMPERATURE_UNIT", {{0, 0, 2, 1}, "INT8_NUMBER", "Temperature unit"}},
    {"COMPARTMENT_0_TEMPERATURE_RANGE", {{0, 128, 1, 1}, "INT16_ARRAY", "Compartment 1 allowed range"}},
    {"COMPARTMENT_1_TEMPERATURE_RANGE", {{16, 128, 1, 1}, "INT16_ARRAY", "Compartment 2 allowed range"}},
    {"COOLER_POWER", {{0, 0, 3, 1}, "INT8_BOOLEAN", "Cooler power"}},
    {"BATTERY_VOLTAGE_LEVEL", {{0, 1, 3, 1}, "INT16_DECICURRENT_VOLT", "Battery voltage"}},
    {"BATTERY_PROTECTION_LEVEL", {{0, 2, 3, 1}, "UINT8_NUMBER", "Battery protection level"}},
    {"POWER_SOURCE", {{0, 5, 3, 1}, "INT8_NUMBER", "Power source"}},
    {"ICEMAKER_POWER", {{0, 6, 3, 1}, "INT8_BOOLEAN", "Icemaker power"}},
    {"COMMUNICATION_ALARM", {{0, 3, 4, 1}, "INT8_BOOLEAN", "Communication alarm"}},
    {"NTC_OPEN_LARGE_ERROR", {{0, 1, 4, 1}, "INT8_BOOLEAN", "NTC open large"}},
    {"NTC_SHORT_LARGE_ERROR", {{0, 2, 4, 1}, "INT8_BOOLEAN", "NTC short large"}},
    {"SOLENOID_VALVE_ERROR", {{0, 9, 4, 1}, "INT8_BOOLEAN", "Solenoid valve error"}},
    {"NTC_OPEN_SMALL_ERROR", {{0, 17, 4, 1}, "INT8_BOOLEAN", "NTC open small"}},
    {"NTC_SHORT_SMALL_ERROR", {{0, 18, 4, 1}, "INT8_BOOLEAN", "NTC short small"}},
    {"FAN_OVERVOLTAGE_ERROR", {{0, 50, 4, 1}, "INT8_BOOLEAN", "Fan overvoltage"}},
    {"COMPRESSOR_START_FAIL_ERROR", {{0, 51, 4, 1}, "INT8_BOOLEAN", "Compressor start fail"}},
    {"COMPRESSOR_SPEED_ERROR", {{0, 52, 4, 1}, "INT8_BOOLEAN", "Compressor speed error"}},
    {"CONTROLLER_OVER_TEMPERATURE", {{0, 53, 4, 1}, "INT8_BOOLEAN", "Controller over temp"}},
    {"TEMPERATURE_ALERT_DCM", {{0, 3, 5, 1}, "INT8_BOOLEAN", "Temp alert DCM"}},
    {"TEMPERATURE_ALERT_CC", {{0, 0, 5, 1}, "INT8_BOOLEAN", "Temp alert CC"}},
    {"DOOR_ALERT", {{0, 1, 5, 1}, "INT8_BOOLEAN", "Door alert"}},
    {"VOLTAGE_ALERT", {{0, 2, 5, 1}, "INT8_BOOLEAN", "Voltage alert"}},
    {"DEVICE_NAME", {{0, 0, 6, 1}, "UTF8_STRING", "Device name"}},
    {"WIFI_MODE", {{0, 1, 6, 1}, "INT8_BOOLEAN", "WiFi mode"}},
    {"BLUETOOTH_MODE", {{0, 3, 6, 1}, "INT8_BOOLEAN", "Bluetooth mode"}},
    {"WIFI_AP_CONNECTED", {{0, 8, 6, 1}, "INT8_BOOLEAN", "WiFi AP connected"}},
    {"STATION_SSID_0", {{0, 0, 7, 1}, "UTF8_STRING", "Station SSID 0"}},
    {"STATION_SSID_1", {{1, 0, 7, 1}, "UTF8_STRING", "Station SSID 1"}},
    {"STATION_SSID_2", {{2, 0, 7, 1}, "UTF8_STRING", "Station SSID 2"}},
    {"STATION_PASSWORD_0", {{0, 1, 7, 1}, "UTF8_STRING", "Station password 0"}},
    {"STATION_PASSWORD_1", {{1, 1, 7, 1}, "UTF8_STRING", "Station password 1"}},
    {"STATION_PASSWORD_2", {{2, 1, 7, 1}, "UTF8_STRING", "Station password 2"}},
    {"STATION_PASSWORD_3", {{3, 1, 7, 1}, "UTF8_STRING", "Station password 3"}},
    {"STATION_PASSWORD_4", {{4, 1, 7, 1}, "UTF8_STRING", "Station password 4"}},
    {"CFX_DIRECT_PASSWORD_0", {{0, 2, 7, 1}, "UTF8_STRING", "CFX direct password 0"}},
    {"CFX_DIRECT_PASSWORD_1", {{1, 2, 7, 1}, "UTF8_STRING", "CFX direct password 1"}},
    {"CFX_DIRECT_PASSWORD_2", {{2, 2, 7, 1}, "UTF8_STRING", "CFX direct password 2"}},
    {"CFX_DIRECT_PASSWORD_3", {{3, 2, 7, 1}, "UTF8_STRING", "CFX direct password 3"}},
    {"CFX_DIRECT_PASSWORD_4", {{4, 2, 7, 1}, "UTF8_STRING", "CFX direct password 4"}},
    {"COMPARTMENT_0_TEMPERATURE_HISTORY_HOUR", {{0, 64, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 1 hour temp history"}},
    {"COMPARTMENT_1_TEMPERATURE_HISTORY_HOUR", {{16, 64, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 2 hour temp history"}},
    {"COMPARTMENT_0_TEMPERATURE_HISTORY_DAY", {{0, 65, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 1 day temp history"}},
    {"COMPARTMENT_1_TEMPERATURE_HISTORY_DAY", {{16, 65, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 2 day temp history"}},
    {"COMPARTMENT_0_TEMPERATURE_HISTORY_WEEK", {{0, 66, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 1 week temp history"}},
    {"COMPARTMENT_1_TEMPERATURE_HISTORY_WEEK", {{16, 66, 1, 1}, "HISTORY_DATA_ARRAY", "Comp 2 week temp history"}},
    {"DC_CURRENT_HISTORY_HOUR", {{0, 64, 3, 1}, "HISTORY_DATA_ARRAY", "DC current hour history"}},
    {"DC_CURRENT_HISTORY_DAY", {{0, 65, 3, 1}, "HISTORY_DATA_ARRAY", "DC current day history"}},
    {"DC_CURRENT_HISTORY_WEEK", {{0, 66, 3, 1}, "HISTORY_DATA_ARRAY", "DC current week history"}},
};

// ----------------- Component lifecycle --------------------------------------

void DometicCfxBle::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Dometic CFX BLE...");
  ESP_LOGCONFIG(TAG, "  Product type: %d", this->product_type_);
  ESP_LOGCONFIG(TAG, "  Temperature unit: %s", this->temperature_unit_.c_str());
  this->apply_temperature_units();
}

void DometicCfxBle::loop() {
  if (!this->connected_ || this->write_handle_ == 0 || this->send_queue_.empty())
    return;

  auto frame = this->send_queue_.front();

  ESP_LOGV(TAG, "TX frame (%u bytes): %s",
           (unsigned) frame.size(),
           format_hex(frame.data(), frame.size()).c_str());

  auto *client = this->parent_;
  if (client == nullptr) {
    ESP_LOGW(TAG, "BLE client parent is null");
    return;
  }

  auto status = esp_ble_gattc_write_char(
      client->get_gattc_if(),
      client->get_conn_id(),
      this->write_handle_,
      frame.size(),
      const_cast<uint8_t *>(frame.data()),
      ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Failed to send frame: %d", status);
    // keep in queue for retry
  } else {
    this->send_queue_.pop();
  }

  this->last_activity_ms_ = millis();
}

void DometicCfxBle::dump_config() {
  ESP_LOGCONFIG(TAG, "Dometic CFX BLE:");
  ESP_LOGCONFIG(TAG, "  Product type: %d", this->product_type_);
  ESP_LOGCONFIG(TAG, "  Temperature unit: %s", this->temperature_unit_.c_str());
}

// ----------------- Frame helpers --------------------------------------------

enum : uint8_t {
  ACTION_PUB   = 0,
  ACTION_SUB   = 1,
  ACTION_PING  = 2,
  ACTION_HELLO = 3,
  ACTION_ACK   = 4,
  ACTION_NAK   = 5,
  ACTION_NOP   = 6,
};

void DometicCfxBle::send_pub(const std::string &topic, const std::vector<uint8_t> &value) {
  auto it = TOPICS.find(topic);
  if (it == TOPICS.end()) {
    ESP_LOGW(TAG, "send_pub: unknown topic '%s'", topic.c_str());
    return;
  }
  const TopicInfo &info = it->second;

  std::vector<uint8_t> frame;
  frame.reserve(1 + 4 + value.size());
  frame.push_back(ACTION_PUB);
  frame.insert(frame.end(), info.param, info.param + 4);
  frame.insert(frame.end(), value.begin(), value.end());

  this->send_queue_.push(std::move(frame));
}

void DometicCfxBle::send_sub(const std::string &topic) {
  auto it = TOPICS.find(topic);
  if (it == TOPICS.end()) {
    ESP_LOGW(TAG, "send_sub: unknown topic '%s'", topic.c_str());
    return;
  }
  const TopicInfo &info = it->second;

  std::vector<uint8_t> frame;
  frame.reserve(1 + 4);
  frame.push_back(ACTION_SUB);
  frame.insert(frame.end(), info.param, info.param + 4);

  this->send_queue_.push(std::move(frame));
}

void DometicCfxBle::send_ping() {
  std::vector<uint8_t> frame(1);
  frame[0] = ACTION_PING;
  this->send_queue_.push(std::move(frame));
}

void DometicCfxBle::send_switch(const std::string &topic, bool value) {
  std::string type_hint = "INT8_BOOLEAN";
  auto it = TOPICS.find(topic);
  if (it != TOPICS.end() && it->second.type != nullptr)
    type_hint = it->second.type;

  auto payload = this->encode_from_bool_(value, type_hint);
  this->send_pub(topic, payload);
}

void DometicCfxBle::send_number(const std::string &topic, float value) {
  std::string type_hint = "INT16_DECIDEGREE_CELSIUS";
  auto it = TOPICS.find(topic);
  if (it != TOPICS.end() && it->second.type != nullptr)
    type_hint = it->second.type;

  auto payload = this->encode_from_float_(value, type_hint);
  this->send_pub(topic, payload);
}

// ----------------- GATTC callbacks (HikeIT-style) ---------------------------

void DometicCfxBle::gattc_event_handler(esp_gattc_cb_event_t event,
                                        esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "GATT open ok");
        this->connected_ = true;
        this->last_activity_ms_ = millis();
        // BLEClient will handle service discovery; nothing to do here.
      } else {
        ESP_LOGW(TAG, "GATT open failed: %d", param->open.status);
      }
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGI(TAG, "Service discovery complete");

      auto *write_chr = this->parent_->get_characteristic(
          esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID),
          esp32_ble_tracker::ESPBTUUID::from_raw(WRITE_UUID));

      auto *notify_chr = this->parent_->get_characteristic(
          esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID),
          esp32_ble_tracker::ESPBTUUID::from_raw(NOTIFY_UUID));

      if (write_chr == nullptr || notify_chr == nullptr) {
        ESP_LOGW(TAG, "Dometic service/characteristics not found");
        this->connected_ = false;
        this->parent_->disconnect();
        return;
      }

      this->write_handle_ = write_chr->handle;
      this->notify_handle_ = notify_chr->handle;

      ESP_LOGI(TAG, "Found write handle=0x%04X notify handle=0x%04X",
               this->write_handle_, this->notify_handle_);

      auto status = esp_ble_gattc_register_for_notify(
          gattc_if,
          this->parent_->get_remote_bda(),
          this->notify_handle_);

      if (status != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register for notifications: %d", status);
      } else {
        this->notify_registered_ = true;
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Notifications registered");

        this->send_ping();

        std::string sub_topic;
        switch (this->product_type_) {
          case 1: sub_topic = "SUBSCRIBE_APP_SZ"; break;
          case 2: sub_topic = "SUBSCRIBE_APP_SZI"; break;
          case 3: sub_topic = "SUBSCRIBE_APP_DZ"; break;
          default:
            ESP_LOGW(TAG, "Unknown product_type %u, not subscribing", this->product_type_);
            break;
        }
        if (!sub_topic.empty())
          this->send_sub(sub_topic);
        
        uint8_t unit_value = (this->temperature_unit_ == "F") ? 1 : 0;
        this->send_pub("PRESENTED_TEMPERATURE_UNIT", {unit_value});

      } else {
        ESP_LOGW(TAG, "REG_FOR_NOTIFY failed: %d", param->reg_for_notify.status);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_)
        break;

      this->handle_notify_(param->notify.value, param->notify.value_len);
      this->last_activity_ms_ = millis();
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected from Dometic CFX device");
      this->connected_ = false;
      this->write_handle_ = 0;
      this->notify_handle_ = 0;
      this->notify_registered_ = false;
      while (!this->send_queue_.empty())
        this->send_queue_.pop();
      break;
    }

    default:
      break;
  }
}

// ----------------- Notification / DDM decode --------------------------------

void DometicCfxBle::handle_notify_(const uint8_t *data, uint16_t length) {
  if (data == nullptr || length == 0)
    return;

  uint8_t action = data[0];
  ESP_LOGVV(TAG, "RX frame action=0x%02X len=%u", action, (unsigned) length);

  auto send_ack = [&]() {
    std::vector<uint8_t> ack(1);
    ack[0] = ACTION_ACK;
    this->send_queue_.push(std::move(ack));
  };

  if (action == ACTION_ACK || action == ACTION_NAK) {
    if (!this->send_queue_.empty())
      this->send_queue_.pop();
    if (action == ACTION_NAK)
      ESP_LOGW(TAG, "Fridge returned NAK");
    return;
  }

  if (action == ACTION_PING || action == ACTION_SUB ||
      action == ACTION_HELLO || action == ACTION_NOP) {
    send_ack();
    return;
  }

  if (action != ACTION_PUB) {
    ESP_LOGV(TAG, "Unhandled DDM action 0x%02X", action);
    return;
  }

  if (length < 5) {
    ESP_LOGW(TAG, "PUB frame too short: %u", (unsigned) length);
    send_ack();
    return;
  }

  uint32_t key = static_cast<uint32_t>(data[1]) |
                 (static_cast<uint32_t>(data[2]) << 8) |
                 (static_cast<uint32_t>(data[3]) << 16) |
                 (static_cast<uint32_t>(data[4]) << 24);

  std::string topic;
  const TopicInfo *info = nullptr;

  for (const auto &kv : TOPICS) {
    const TopicInfo &ti = kv.second;
    uint32_t tk = static_cast<uint32_t>(ti.param[0]) |
                  (static_cast<uint32_t>(ti.param[1]) << 8) |
                  (static_cast<uint32_t>(ti.param[2]) << 16) |
                  (static_cast<uint32_t>(ti.param[3]) << 24);
    if (tk == key) {
      topic = kv.first;
      info = &ti;
      break;
    }
  }

  std::vector<uint8_t> payload;
  if (length > 5)
    payload.assign(data + 5, data + length);

  if (info == nullptr) {
    ESP_LOGV(TAG, "Unknown DDM key 0x%08X", (unsigned) key);
    send_ack();
    return;
  }

  ESP_LOGV(TAG, "PUB %s (%s) len=%u",
           topic.c_str(),
           info->type ? info->type : "",
           (unsigned) payload.size());

  this->update_entity_(topic, payload);

  std::string desc = this->get_english_desc_(topic, *info, payload);
  if (!desc.empty())
    ESP_LOGD(TAG, "%s", desc.c_str());

  send_ack();
}

// ----------------- Entity update + encode/decode ----------------------------

void DometicCfxBle::update_entity_(const std::string &topic, const std::vector<uint8_t> &value) {
  std::string type_hint = "RAW";
  auto ti = TOPICS.find(topic);
  if (ti != TOPICS.end() && ti->second.type != nullptr)
    type_hint = ti->second.type;

  if (auto it = sensors_.find(topic); it != sensors_.end()) {
    float v = this->decode_to_float_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = binary_sensors_.find(topic); it != binary_sensors_.end()) {
    bool v = this->decode_to_bool_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = switches_.find(topic); it != switches_.end()) {
    bool v = this->decode_to_bool_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = numbers_.find(topic); it != numbers_.end()) {
    float v = this->decode_to_float_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = text_sensors_.find(topic); it != text_sensors_.end()) {
    std::string s = this->decode_to_string_(value, type_hint);
    it->second->publish_state(s);
    return;
  }

  ESP_LOGV(TAG, "No entity for topic '%s'", topic.c_str());
}

float DometicCfxBle::decode_to_float_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  if (type_hint == "INT16_DECIDEGREE_CELSIUS") {
    if (bytes.size() < 2) return NAN;
    int16_t raw = static_cast<int16_t>(bytes[0] | (static_cast<int16_t>(bytes[1]) << 8));
    float celsius = static_cast<float>(raw) / 10.0f;
    if (this->temperature_unit_ == "F") {
      return (celsius * 9.0 / 5.0) + 32.0;
    }
    return celsius;
  }

  if (type_hint == "INT16_DECICURRENT_VOLT") {
    if (bytes.size() < 2) return NAN;
    uint16_t raw = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
    return static_cast<float>(raw) / 10.0f;
  }

  if (type_hint == "INT8_NUMBER" || type_hint == "UINT8_NUMBER") {
    if (bytes.empty()) return NAN;
    return static_cast<float>(bytes[0]);
  }

  return NAN;
}

bool DometicCfxBle::decode_to_bool_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  if (bytes.empty()) return false;
  if (type_hint == "INT8_BOOLEAN")
    return bytes[0] != 0;
  return bytes[0] != 0;
}

std::string DometicCfxBle::decode_to_string_(const std::vector<uint8_t> &bytes,
                                             const std::string &type_hint) {
  if (type_hint == "UTF8_STRING") {
    if (bytes.empty()) return "";
    size_t end = 0;
    while (end < bytes.size() && end < 15 && bytes[end] != 0x00)
      end++;
    return std::string(reinterpret_cast<const char *>(bytes.data()), end);
  }

  char buf[4];
  std::string out;
  out.reserve(bytes.size() * 2);
  for (auto b : bytes) {
    snprintf(buf, sizeof(buf), "%02X", static_cast<unsigned>(b));
    out.append(buf);
  }
  return out;
}

std::vector<uint8_t> DometicCfxBle::encode_from_bool_(bool value, const std::string &type_hint) {
  std::vector<uint8_t> out;
  (void) type_hint;
  out.push_back(static_cast<uint8_t>(value ? 1 : 0));
  return out;
}

std::vector<uint8_t> DometicCfxBle::encode_from_float_(float value, const std::string &type_hint) {
  std::vector<uint8_t> out;

  if (type_hint == "INT16_DECIDEGREE_CELSIUS") {
    float celsius = value;
    if (this->temperature_unit_ == "F") {
        celsius = (value - 32.0) * 5.0 / 9.0;
    }
    int16_t deci = static_cast<int16_t>(std::lround(celsius * 10.0f));
    out.push_back(static_cast<uint8_t>(deci & 0xFF));
    out.push_back(static_cast<uint8_t>((deci >> 8) & 0xFF));
    return out;
  }

  if (type_hint == "INT16_DECICURRENT_VOLT") {
    uint16_t deci = static_cast<uint16_t>(std::lround(value * 10.0f));
    out.push_back(static_cast<uint8_t>(deci & 0xFF));
    out.push_back(static_cast<uint8_t>((deci >> 8) & 0xFF));
    return out;
  }

  if (type_hint == "INT8_NUMBER" || type_hint == "UINT8_NUMBER") {
    int v = static_cast<int>(std::lround(value));
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    out.push_back(static_cast<uint8_t>(v));
    return out;
  }

  int v = static_cast<int>(std::lround(value));
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  out.push_back(static_cast<uint8_t>(v));
  return out;
}

std::string DometicCfxBle::get_english_desc_(const std::string &topic_key,
                                             const TopicInfo &info,
                                             const std::vector<uint8_t> &bytes) {
  const std::string type(info.type ? info.type : "");
  const std::string desc(info.description ? info.description : "");

  if (type == "INT16_DECIDEGREE_CELSIUS") {
    float v = decode_to_float_(bytes, type);
    if (v == NO_VALUE)
      return desc + " is unavailable";
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %.1f°%s", desc.c_str(), v, this->temperature_unit_.c_str());
    return std::string(buf);
  }

  if (type == "INT8_BOOLEAN") {
    bool v = decode_to_bool_(bytes, type);
    std::string lower = desc;
    for (auto &c : lower) c = static_cast<char>(tolower(c));
    const char *state_str = nullptr;
    if (lower.find("power") != std::string::npos)
      state_str = v ? "on" : "off";
    else
      state_str = v ? "active" : "inactive";
    return desc + " is " + state_str;
  }

  if (type == "UINT8_NUMBER" && topic_key == "BATTERY_PROTECTION_LEVEL") {
    if (bytes.empty()) return "";
    int v = bytes[0];
    const char *label = battery_level_str(v);
    if (label != nullptr)
      return desc + " is " + label;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "INT8_NUMBER" && topic_key == "POWER_SOURCE") {
    if (bytes.empty()) return "";
    int v = bytes[0];
    const char *label = power_source_str(v);
    if (label != nullptr)
      return desc + " is " + label;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "UTF8_STRING") {
    std::string v = decode_to_string_(bytes, type);
    return desc + " is " + v;
  }

  if (type == "INT16_ARRAY") {
    if (bytes.size() < 4) return "";
    std::vector<uint8_t> b0(bytes.begin(), bytes.begin() + 2);
    std::vector<uint8_t> b1(bytes.begin() + 2, bytes.begin() + 4);
    float min_v = decode_to_float_(b0, "INT16_DECIDEGREE_CELSIUS");
    float max_v = decode_to_float_(b1, "INT16_DECIDEGREE_CELSIUS");
    char buf[96];
    snprintf(buf, sizeof(buf), "%s is %.1f to %.1f°%s", desc.c_str(), min_v, max_v, this->temperature_unit_.c_str());
    return std::string(buf);
  }

  if (type == "HISTORY_DATA_ARRAY") {
    if (bytes.size() < 15) return "";
    std::string line = desc + ": temps [";
    char buf[64];
    for (int i = 0; i < 7; i++) {
      std::vector<uint8_t> b(bytes.begin() + i * 2, bytes.begin() + i * 2 + 2);
      float t = decode_to_float_(b, "INT16_DECIDEGREE_CELSIUS");
      if (i != 0) line += ", ";
      snprintf(buf, sizeof(buf), "%.1f°%s", t, this->temperature_unit_.c_str());
      line += buf;
    }
    uint8_t ts = bytes[14];
    snprintf(buf, sizeof(buf), "], timestamp %u", (unsigned) ts);
    line += buf;
    return line;
  }

  if (type == "INT16_DECICURRENT_VOLT") {
    float v = decode_to_float_(bytes, type);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %.1fV", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "INT8_NUMBER") {
    if (bytes.empty()) return "";
    int v = bytes[0];
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

  return "";
}


// ----------------- Wrapper entity methods -----------------------------------

void DometicCfxBleSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Switch has no parent");
    this->publish_state(state);
    return;
  }

  this->parent_->send_switch(this->topic_, state);
  this->publish_state(state);
}

void DometicCfxBleNumber::control(float value) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Number has no parent");
    this->publish_state(value);
    return;
  }

  this->parent_->send_number(this->topic_, value);
  this->publish_state(value);
}

}  // namespace dometic_cfx_ble
}  // namespace esphome
