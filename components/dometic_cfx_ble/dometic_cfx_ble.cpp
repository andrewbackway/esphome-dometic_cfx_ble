#include "dometic_cfx_ble.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

extern "C" {
#include "esp_gattc_api.h"
}

#include <array>

namespace esphome {
namespace dometic_cfx_ble {

// UUID strings (from Dometic app)
// NOTE: this is the CFX5 "MC1" BLE generation. The original CFX3 generation
// used the 537a03xx group instead (537a0300 / ...0301 / ...0302).
static const char *SERVICE_UUID = "537a0400-0995-481f-926c-1604e23fd515";
static const char *WRITE_UUID   = "537a0401-0995-481f-926c-1604e23fd515";
static const char *NOTIFY_UUID  = "537a0402-0995-481f-926c-1604e23fd515";

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

// ----------------------------------------------------------------------------
// CFX5 "MC1" generation parameter map.
//
// Reverse-engineered against a CFX5 95DZ (dual-zone) in the Rotoslider/
// dometic-cfx5-monitor project (github.com/Rotoslider/dometic-cfx5-monitor).
// The group byte 0x1A is CONFIRMED ONLY on that 95DZ unit. Single-zone CFX5
// models (25/45/75 etc.) very likely use the same group byte and just never
// populate the "zone 1" half of the payload, but this is NOT verified yet.
//
// Recommended first step on real hardware: leave DUMP_UNKNOWN_FRAMES (below)
// enabled, watch the logs for a few minutes, and confirm the keys below
// actually show up. If a key never appears, or an extra unknown key does,
// adjust this table accordingly before trusting the decoded values.
//
// Dual-zone parameters arrive as ONE 8-byte notify payload = two little-
// endian int32 values (zone0, zone1), each already scaled /1000. Single-zone
// coolers likely send only the first 4 bytes (zone0).
const std::map<std::string, TopicInfo> TOPICS = {
    // measured_temp: key (0x04,0x00,0x00,0x1A)
    {"COMPARTMENT_0_MEASURED_TEMPERATURE", {{0x04, 0x00, 0x00, 0x1A}, 0, "PAIR_INT32_MILLI_CELSIUS", "Compartment 1 current temp"}},
    {"COMPARTMENT_1_MEASURED_TEMPERATURE", {{0x04, 0x00, 0x00, 0x1A}, 1, "PAIR_INT32_MILLI_CELSIUS", "Compartment 2 current temp"}},

    // set_temp: key (0x05,0x00,0x00,0x1A)
    {"COMPARTMENT_0_SET_TEMPERATURE", {{0x05, 0x00, 0x00, 0x1A}, 0, "PAIR_INT32_MILLI_CELSIUS", "Compartment 1 set temp"}},
    {"COMPARTMENT_1_SET_TEMPERATURE", {{0x05, 0x00, 0x00, 0x1A}, 1, "PAIR_INT32_MILLI_CELSIUS", "Compartment 2 set temp"}},

    // compressor (pair_bool): key (0x03,0x00,0x00,0x1A)
    {"COMPARTMENT_0_COMPRESSOR", {{0x03, 0x00, 0x00, 0x1A}, 0, "PAIR_INT32_BOOLEAN", "Compartment 1 compressor"}},
    {"COMPARTMENT_1_COMPRESSOR", {{0x03, 0x00, 0x00, 0x1A}, 1, "PAIR_INT32_BOOLEAN", "Compartment 2 compressor"}},

    // door_open: key (0x07,0x00,0x00,0x1A) - width unconfirmed, treated as pair
    {"COMPARTMENT_0_DOOR_OPEN", {{0x07, 0x00, 0x00, 0x1A}, 0, "PAIR_INT32_BOOLEAN", "Compartment 1 door open"}},
    {"COMPARTMENT_1_DOOR_OPEN", {{0x07, 0x00, 0x00, 0x1A}, 1, "PAIR_INT32_BOOLEAN", "Compartment 2 door open"}},

    // compartment_power: key (0x0B,0x00,0x00,0x1A) - width unconfirmed
    {"COMPARTMENT_POWER", {{0x0B, 0x00, 0x00, 0x1A}, 0, "INT32_NUMBER", "Compartment power"}},

    // dc_voltage: key (0x0C,0x00,0x00,0x1A), single int32/1000 V
    {"DC_VOLTAGE", {{0x0C, 0x00, 0x00, 0x1A}, 0, "INT32_MILLI_VOLT", "DC input voltage"}},

    // power_source: key (0x10,0x00,0x00,0x1A), 0=AC/1=DC/2=Solar
    {"POWER_SOURCE", {{0x10, 0x00, 0x00, 0x1A}, 0, "INT32_POWER_SOURCE", "Power source"}},

    // door_alert: key (0x12,0x00,0x00,0x1A), single bool
    {"DOOR_ALERT", {{0x12, 0x00, 0x00, 0x1A}, 0, "INT32_BOOLEAN", "Door alert"}},
};

// ----------------- Component lifecycle --------------------------------------

void DometicCfxBle::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Dometic CFX BLE...");
  ESP_LOGCONFIG(TAG, "  Product type: %d", this->product_type_);
  ESP_LOGCONFIG(TAG, "  Temperature unit: %s", this->temperature_unit_.c_str());
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

// CFX5 "MC1" generation opcodes (confirmed via Rotoslider/dometic-cfx5-monitor
// against real hardware). There is no ACK/NAK/HELLO handshake on this
// generation: you write a SUBSCRIBE frame per parameter and the cooler just
// starts pushing PUBLISH frames for it, forever, on its own.
enum : uint8_t {
  ACTION_PUB = 0x10,  // cooler -> us: [0x10, key0..3, value...]
  ACTION_SUB = 0x12,  // us -> cooler: [0x12, key0..3]
};

// Set to 1 while bringing up a new/unverified CFX5 model: logs every
// PUBLISH frame whose key isn't in TOPICS yet, in hex, at INFO level so it's
// visible without needing VERBOSE logging. Turn back to 0 once your model's
// keys are confirmed and added to TOPICS.
#define DOMETIC_DUMP_UNKNOWN_FRAMES 1

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
  // No-op: the CFX5 "MC1" generation has no ping/keepalive frame in its
  // protocol - kept only so the header's public API stays unchanged.
}

void DometicCfxBle::send_switch(const std::string &topic, bool value) {
  // NOTE: writing (e.g. toggling cooler/compartment power) has NOT been
  // reverse-engineered for the CFX5 "MC1" generation yet - only reading via
  // 0x10/0x12 has been confirmed. Sending an unverified PUB (opcode 0) frame
  // here could be ignored, or in the worst case misinterpreted by the
  // cooler's firmware. Disabled until confirmed against a real device.
  ESP_LOGW(TAG, "send_switch('%s'): write support unverified for CFX5 - not sending", topic.c_str());
}

void DometicCfxBle::send_number(const std::string &topic, float value) {
  // See send_switch() note above - same caveat applies to setpoint writes.
  ESP_LOGW(TAG, "send_number('%s'): write support unverified for CFX5 - not sending", topic.c_str());
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

        // CFX5 has no single "subscribe all" command like the CFX3 DDM
        // protocol did. Instead: send one SUBSCRIBE (0x12) frame per unique
        // parameter key. Two topics can share the same key (zone0/zone1 of
        // a dual-zone parameter), so de-duplicate by key first.
        std::vector<std::array<uint8_t, 4>> unique_keys;
        for (const auto &kv : TOPICS) {
          std::array<uint8_t, 4> key = {kv.second.param[0], kv.second.param[1],
                                         kv.second.param[2], kv.second.param[3]};
          bool seen = false;
          for (const auto &k : unique_keys) {
            if (k == key) { seen = true; break; }
          }
          if (!seen)
            unique_keys.push_back(key);
        }

        ESP_LOGD(TAG, "Subscribing to %u parameters", (unsigned) unique_keys.size());
        for (const auto &key : unique_keys) {
          std::vector<uint8_t> frame;
          frame.reserve(5);
          frame.push_back(ACTION_SUB);
          frame.insert(frame.end(), key.begin(), key.end());
          this->send_queue_.push(std::move(frame));
        }

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

  if (action != ACTION_PUB) {
    // CFX5 has no ACK/NAK/HELLO/PING to answer to - anything that isn't a
    // 0x10 publish is either unexpected or a generation we haven't seen yet.
    ESP_LOGV(TAG, "Unhandled frame action=0x%02X len=%u: %s",
             action, (unsigned) length, format_hex(data, length).c_str());
    return;
  }

  if (length < 5) {
    ESP_LOGW(TAG, "PUB frame too short: %u", (unsigned) length);
    return;
  }

  uint32_t key = static_cast<uint32_t>(data[1]) |
                 (static_cast<uint32_t>(data[2]) << 8) |
                 (static_cast<uint32_t>(data[3]) << 16) |
                 (static_cast<uint32_t>(data[4]) << 24);

  std::vector<uint8_t> payload;
  if (length > 5)
    payload.assign(data + 5, data + length);

  bool matched = false;
  for (const auto &kv : TOPICS) {
    const std::string &topic = kv.first;
    const TopicInfo &info = kv.second;
    uint32_t tk = static_cast<uint32_t>(info.param[0]) |
                  (static_cast<uint32_t>(info.param[1]) << 8) |
                  (static_cast<uint32_t>(info.param[2]) << 16) |
                  (static_cast<uint32_t>(info.param[3]) << 24);
    if (tk != key)
      continue;

    matched = true;

    // Dual-zone parameters pack zone0/zone1 into one 8-byte payload; slice
    // out this topic's 4 bytes if the payload is wide enough, otherwise
    // (single-zone hardware) fall back to the whole payload for zone 0.
    std::vector<uint8_t> slice;
    std::string type_hint = info.type ? info.type : "RAW";
    if (type_hint.rfind("PAIR_", 0) == 0 && payload.size() >= 8) {
      size_t off = info.zone == 1 ? 4 : 0;
      slice.assign(payload.begin() + off, payload.begin() + off + 4);
    } else {
      slice = payload;
    }

    ESP_LOGV(TAG, "PUB %s (%s) raw=%s", topic.c_str(), type_hint.c_str(),
             format_hex(slice.data(), slice.size()).c_str());

    this->update_entity_(topic, slice);

    std::string desc = this->get_english_desc_(topic, info, slice);
    if (!desc.empty())
      ESP_LOGD(TAG, "%s", desc.c_str());
  }

  if (!matched) {
#if DOMETIC_DUMP_UNKNOWN_FRAMES
    ESP_LOGI(TAG, "Unknown key 0x%08X (%u bytes): %s", (unsigned) key,
             (unsigned) payload.size(), format_hex(payload.data(), payload.size()).c_str());
#else
    ESP_LOGV(TAG, "Unknown key 0x%08X", (unsigned) key);
#endif
  }
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

static int32_t decode_i32_le(const std::vector<uint8_t> &bytes) {
  if (bytes.size() < 4) return 0;
  uint32_t raw = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                 (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
  return static_cast<int32_t>(raw);
}

float DometicCfxBle::decode_to_float_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  // CFX5 "MC1" generation types: 4-byte little-endian int32, pre-scaled /1000.
  if (type_hint == "PAIR_INT32_MILLI_CELSIUS" || type_hint == "INT32_MILLI_CELSIUS") {
    if (bytes.size() < 4) return NAN;
    float celsius = static_cast<float>(decode_i32_le(bytes)) / 1000.0f;
    if (this->temperature_unit_ == "F")
      return (celsius * 9.0f / 5.0f) + 32.0f;
    return celsius;
  }

  if (type_hint == "INT32_MILLI_VOLT") {
    if (bytes.size() < 4) return NAN;
    return static_cast<float>(decode_i32_le(bytes)) / 1000.0f;
  }

  if (type_hint == "INT32_NUMBER" || type_hint == "INT32_POWER_SOURCE") {
    if (bytes.size() < 4) return NAN;
    return static_cast<float>(decode_i32_le(bytes));
  }

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
  if (type_hint == "PAIR_INT32_BOOLEAN" || type_hint == "INT32_BOOLEAN") {
    if (bytes.size() < 4) return false;
    return decode_i32_le(bytes) != 0;
  }
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

  if (type == "PAIR_INT32_MILLI_CELSIUS" || type == "INT32_MILLI_CELSIUS") {
    float v = decode_to_float_(bytes, type);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %.1f\u00b0%s", desc.c_str(), v, this->temperature_unit_.c_str());
    return std::string(buf);
  }

  if (type == "INT32_MILLI_VOLT") {
    float v = decode_to_float_(bytes, type);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %.2fV", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "PAIR_INT32_BOOLEAN" || type == "INT32_BOOLEAN") {
    bool v = decode_to_bool_(bytes, type);
    return desc + " is " + (v ? "active" : "inactive");
  }

  if (type == "INT32_POWER_SOURCE") {
    int v = static_cast<int>(decode_to_float_(bytes, type));
    const char *label = power_source_str(v);
    if (label != nullptr)
      return desc + " is " + label;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "INT32_NUMBER") {
    int v = static_cast<int>(decode_to_float_(bytes, type));
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

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
