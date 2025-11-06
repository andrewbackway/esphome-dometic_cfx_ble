#include "dometic_cfx_ble.h"

#include <esp_err.h>
#include <esp_bt.h>

namespace esphome {
namespace dometic_cfx_ble {

// UUIDs (same as SERVICE_UUID / WRITE_UUID / NOTIFY_UUID from test.py)
static const uint8_t SERVICE_UUID_128[16] = {
    // LSB -> MSB of 537a0300-0995-481f-926c-1604e23fd515
    0x15, 0xd5, 0x3f, 0xe2, 0x04, 0x16, 0x6c, 0x92,
    0x1f, 0x48, 0x95, 0x09, 0x00, 0x03, 0x7a, 0x53,
};

static const uint8_t WRITE_CHAR_UUID_128[16] = {
    // 537a0301-0995-481f-926c-1604e23fd515
    0x15, 0xd5, 0x3f, 0xe2, 0x04, 0x16, 0x6c, 0x92,
    0x1f, 0x48, 0x95, 0x09, 0x01, 0x03, 0x7a, 0x53,
};

static const uint8_t NOTIFY_CHAR_UUID_128[16] = {
    // 537a0302-0995-481f-926c-1604e23fd515
    0x15, 0xd5, 0x3f, 0xe2, 0x04, 0x16, 0x6c, 0x92,
    0x1f, 0x48, 0x95, 0x09, 0x02, 0x03, 0x7a, 0x53,
};

// Battery / power strings from test.py
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

// -------- FULL TOPIC TABLE (mirrors TOPICS in test.py) --------

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

DometicCfxBle *DometicCfxBle::instance_ = nullptr;

// ----------------- Basic setup / loop ---------------------------------------

void DometicCfxBle::set_mac_address(const uint8_t *mac) {
  if (mac == nullptr) return;
  std::memcpy(this->mac_address_, mac, 6);
}

void DometicCfxBle::setup() {
  ESP_LOGI(TAG, "Initializing Dometic CFX3 BLE hub");

  instance_ = this;

  esp_err_t err;

  err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "esp_bt_controller_mem_release failed: %d", err);
  }

  err = esp_ble_gap_register_callback(&DometicCfxBle::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
  }

  err = esp_ble_gattc_register_callback(&DometicCfxBle::gattc_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_register_callback failed: %d", err);
  }

  err = esp_ble_gattc_app_register(0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_app_register failed: %d", err);
  }

  this->start_scan_();
}

void DometicCfxBle::dump_config() {
  ESP_LOGCONFIG(TAG, "Dometic CFX3 BLE:");
  ESP_LOGCONFIG(TAG, "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
                mac_address_[0], mac_address_[1], mac_address_[2],
                mac_address_[3], mac_address_[4], mac_address_[5]);
  ESP_LOGCONFIG(TAG, "  Product type: %u", product_type_);
}

void DometicCfxBle::loop() {
  const uint32_t now = millis();

  if (connected_ && (now - last_activity_ms_ > 15000U)) {
    this->send_ping();
  }

  if (connected_ && write_handle_ != 0 && !send_queue_.empty()) {
    auto packet = std::move(send_queue_.front());
    send_queue_.pop();

    esp_err_t err = esp_ble_gattc_write_char(
        gattc_if_, conn_id_, write_handle_,
        static_cast<uint16_t>(packet.size()), packet.data(),
        ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_write_char failed: %d", err);
    } else {
      last_activity_ms_ = now;
    }
  }
}

// ----------------- Frame-level helpers --------------------------------------

void DometicCfxBle::send_pub(const std::string &topic, const std::vector<uint8_t> &value) {
  auto it = TOPICS.find(topic);
  if (it == TOPICS.end()) {
    ESP_LOGW(TAG, "send_pub: unknown topic '%s'", topic.c_str());
    return;
  }
  const TopicInfo &info = it->second;

  std::vector<uint8_t> packet;
  packet.reserve(1 + 4 + value.size());
  packet.push_back(ACTION_PUB);
  packet.insert(packet.end(), info.param, info.param + 4);
  packet.insert(packet.end(), value.begin(), value.end());

  send_queue_.push(std::move(packet));
}

void DometicCfxBle::send_sub(const std::string &topic) {
  auto it = TOPICS.find(topic);
  if (it == TOPICS.end()) {
    ESP_LOGW(TAG, "send_sub: unknown topic '%s'", topic.c_str());
    return;
  }
  const TopicInfo &info = it->second;

  std::vector<uint8_t> packet;
  packet.reserve(1 + 4);
  packet.push_back(ACTION_SUB);
  packet.insert(packet.end(), info.param, info.param + 4);

  send_queue_.push(std::move(packet));
}

void DometicCfxBle::send_ping() {
  std::vector<uint8_t> packet(1);
  packet[0] = ACTION_PING;
  send_queue_.push(std::move(packet));
}

void DometicCfxBle::send_switch(const std::string &topic, bool value) {
  std::string type_hint = "INT8_BOOLEAN";
  auto it = TOPICS.find(topic);
  if (it != TOPICS.end() && it->second.type != nullptr) {
    type_hint = it->second.type;
  }
  auto payload = this->encode_from_bool_(value, type_hint);
  this->send_pub(topic, payload);
}

void DometicCfxBle::send_number(const std::string &topic, float value) {
  std::string type_hint = "INT16_DECIDEGREE_CELSIUS";
  auto it = TOPICS.find(topic);
  if (it != TOPICS.end() && it->second.type != nullptr) {
    type_hint = it->second.type;
  }
  auto payload = this->encode_from_float_(value, type_hint);
  this->send_pub(topic, payload);
}

// ----------------- GAP / GATTC plumbing -------------------------------------

void DometicCfxBle::start_scan_() {
  if (scan_in_progress_) return;

  esp_ble_scan_params_t scan_params{};
  scan_params.scan_type          = BLE_SCAN_TYPE_ACTIVE;
  scan_params.own_addr_type      = BLE_ADDR_TYPE_PUBLIC;
  scan_params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scan_params.scan_interval      = 0x50;
  scan_params.scan_window        = 0x30;
  scan_params.scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE;

  esp_err_t err = esp_ble_gap_set_scan_params(&scan_params);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %d", err);
    return;
  }

  scan_in_progress_ = true;
}

void DometicCfxBle::connect_() {
  if (gattc_if_ == ESP_GATT_IF_NONE) {
    ESP_LOGW(TAG, "connect_: GATTC interface not ready yet");
    return;
  }

  ESP_LOGI(TAG, "Opening GATT connection to CFX3");

  esp_err_t err = esp_ble_gattc_open(
      gattc_if_, mac_address_, BLE_ADDR_TYPE_PUBLIC, true);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_open failed: %d", err);
  }
}

void DometicCfxBle::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (instance_ != nullptr) {
    instance_->handle_gap_event(event, param);
  }
}

void DometicCfxBle::gattc_event_handler(esp_gattc_cb_event_t event,
                                        esp_gatt_if_t gatt_if,
                                        esp_ble_gattc_cb_param_t *param) {
  if (instance_ != nullptr) {
    instance_->handle_gattc_event(event, gatt_if, param);
  }
}

void DometicCfxBle::handle_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      ESP_LOGI(TAG, "BLE scan params set, starting scan");
      esp_ble_gap_start_scanning(30);
      break;
    }

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      auto &r = param->scan_rst;
      if (r.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT)
        break;

      if (std::memcmp(r.bda, mac_address_, 6) == 0) {
        ESP_LOGI(TAG, "Found target CFX3, stopping scan and connecting");
        esp_ble_gap_stop_scanning();
        this->connect_();
      }
      break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      ESP_LOGD(TAG, "BLE scan stopped");
      scan_in_progress_ = false;
      break;
    }

    default:
      break;
  }
}

void DometicCfxBle::handle_gattc_event(esp_gattc_cb_event_t event,
                                       esp_gatt_if_t gatt_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT: {
      gattc_if_ = gatt_if;
      ESP_LOGI(TAG, "GATTC registered, app_id=%d", param->reg.app_id);
      break;
    }

    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        conn_id_ = param->open.conn_id;
        connected_ = true;
        last_activity_ms_ = millis();
        ESP_LOGI(TAG, "GATTC connected, conn_id=%d", conn_id_);

        esp_bt_uuid_t service_uuid{};
        service_uuid.len = ESP_UUID_LEN_128;
        std::memcpy(service_uuid.uuid.uuid128, SERVICE_UUID_128, sizeof(SERVICE_UUID_128));
        esp_ble_gattc_search_service(gatt_if, conn_id_, &service_uuid);
      } else {
        ESP_LOGW(TAG, "GATTC open failed, status=%d", param->open.status);
      }
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      esp_bt_uuid_t write_uuid{};
      write_uuid.len = ESP_UUID_LEN_128;
      std::memcpy(write_uuid.uuid.uuid128, WRITE_CHAR_UUID_128, sizeof(WRITE_CHAR_UUID_128));

      esp_bt_uuid_t notify_uuid{};
      notify_uuid.len = ESP_UUID_LEN_128;
      std::memcpy(notify_uuid.uuid.uuid128, NOTIFY_CHAR_UUID_128, sizeof(NOTIFY_CHAR_UUID_128));

      esp_gattc_char_elem_t char_elem[2];
      uint16_t count = 0;

      if (esp_ble_gattc_get_char_by_uuid(
              gatt_if, conn_id_, 0, 0xFFFF, write_uuid, char_elem, &count) == ESP_OK &&
          count > 0) {
        write_handle_ = char_elem[0].char_handle;
      }

      count = 0;
      if (esp_ble_gattc_get_char_by_uuid(
              gatt_if, conn_id_, 0, 0xFFFF, notify_uuid, char_elem, &count) == ESP_OK &&
          count > 0) {
        notify_handle_ = char_elem[0].char_handle;
      }

      if (notify_handle_ != 0) {
        uint8_t cccd[2] = {0x01, 0x00};
        esp_err_t err = esp_ble_gattc_write_char_descr(
            gatt_if, conn_id_, notify_handle_ + 1, sizeof(cccd), cccd,
            ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        if (err != ESP_OK) {
          ESP_LOGW(TAG, "Failed to enable notifications, err=%d", err);
        }
      }

      this->send_ping();
      std::string sub_key =
          (product_type_ == 1) ? "SUBSCRIBE_APP_SZ"
                               : (product_type_ == 2) ? "SUBSCRIBE_APP_SZI" : "SUBSCRIBE_APP_DZ";
      this->send_sub(sub_key);
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.is_notify &&
          param->notify.value != nullptr &&
          param->notify.value_len > 0) {
        this->handle_notify_(param->notify.value, param->notify.value_len);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "GATTC disconnected, reason=%d", param->disconnect.reason);
      connected_ = false;
      write_handle_ = 0;
      notify_handle_ = 0;
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write char status=%d", param->write.status);
      }
      break;
    }

    default:
      break;
  }
}

// ----------------- DDM frame handling ---------------------------------------

void DometicCfxBle::handle_notify_(const uint8_t *data, size_t len) {
  if (data == nullptr || len < 1) return;

  uint8_t action = data[0];
  ESP_LOGVV(TAG, "Notify frame: action=0x%02X len=%u", action, (unsigned) len);

  // ACK / NAK for our queued frames
  if (action == ACTION_ACK || action == ACTION_NAK) {
    if (!send_queue_.empty()) {
      send_queue_.pop();
    }
    if (action == ACTION_NAK) {
      ESP_LOGW(TAG, "Fridge returned NAK");
    }
    last_activity_ms_ = millis();
    return;
  }

  // Always ACK incoming PUB/PING/SUB/NOP/HELLO just like test.py
  auto send_ack = [&]() {
    if (write_handle_ == 0) return;
    uint8_t ack = ACTION_ACK;
    esp_ble_gattc_write_char(
        gattc_if_, conn_id_, write_handle_, 1, &ack,
        ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  };

  if (action == ACTION_PING || action == ACTION_SUB ||
      action == ACTION_NOP || action == ACTION_HELLO) {
    ESP_LOGV(TAG, "DDM control frame action=0x%02X", action);
    send_ack();
    last_activity_ms_ = millis();
    return;
  }

  if (action != ACTION_PUB) {
    ESP_LOGV(TAG, "Unhandled DDM action 0x%02X", action);
    last_activity_ms_ = millis();
    return;
  }

  if (len < 5) {
    ESP_LOGW(TAG, "PUB frame too short: %u", (unsigned) len);
    send_ack();
    return;
  }

  uint32_t key = static_cast<uint32_t>(data[1]) |
                 (static_cast<uint32_t>(data[2]) << 8) |
                 (static_cast<uint32_t>(data[3]) << 16) |
                 (static_cast<uint32_t>(data[4]) << 24);

  std::string topic;
  const TopicInfo *topic_info = nullptr;

  for (const auto &kv : TOPICS) {
    const TopicInfo &info = kv.second;
    uint32_t tk = static_cast<uint32_t>(info.param[0]) |
                  (static_cast<uint32_t>(info.param[1]) << 8) |
                  (static_cast<uint32_t>(info.param[2]) << 16) |
                  (static_cast<uint32_t>(info.param[3]) << 24);
    if (tk == key) {
      topic = kv.first;
      topic_info = &info;
      break;
    }
  }

  std::vector<uint8_t> payload;
  if (len > 5) {
    payload.assign(data + 5, data + len);
  }

  if (topic_info == nullptr) {
    ESP_LOGV(TAG, "Unknown DDM param key 0x%08X (len=%u)", (unsigned) key, (unsigned) len);
    send_ack();
    last_activity_ms_ = millis();
    return;
  }

  ESP_LOGV(TAG, "PUB %s (%s) payload_len=%u",
           topic.c_str(),
           topic_info->type ? topic_info->type : "",
           (unsigned) payload.size());

  this->update_entity_(topic, payload);

  std::string desc = this->get_english_desc_(topic, *topic_info, payload);
  if (!desc.empty()) {
    ESP_LOGD(TAG, "%s", desc.c_str());
  }

  send_ack();
  last_activity_ms_ = millis();
}

void DometicCfxBle::update_entity_(const std::string &topic, const std::vector<uint8_t> &value) {
  std::string type_hint = "RAW";
  auto ti = TOPICS.find(topic);
  if (ti != TOPICS.end() && ti->second.type != nullptr) {
    type_hint = ti->second.type;
  }

  if (auto it = sensors_.find(topic); it != sensors_.end()) {
    float v = decode_to_float_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = binary_sensors_.find(topic); it != binary_sensors_.end()) {
    bool v = decode_to_bool_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = switches_.find(topic); it != switches_.end()) {
    bool v = decode_to_bool_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = numbers_.find(topic); it != numbers_.end()) {
    float v = decode_to_float_(value, type_hint);
    it->second->publish_state(v);
    return;
  }

  if (auto it = text_sensors_.find(topic); it != text_sensors_.end()) {
    std::string s = decode_to_string_(value, type_hint);
    it->second->publish_state(s);
    return;
  }

  ESP_LOGV(TAG, "No entity registered for topic '%s'", topic.c_str());
}

// ----------------- Typed encode / decode (mirror test.py) -------------------

float DometicCfxBle::decode_to_float_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  if (type_hint == "INT16_DECIDEGREE_CELSIUS") {
    if (bytes.size() < 2) return NAN;
    int16_t val = static_cast<int16_t>(bytes[0] | (static_cast<int16_t>(bytes[1]) << 8));
    return static_cast<float>(val) / 10.0f;
  }

  if (type_hint == "INT16_DECICURRENT_VOLT") {
    if (bytes.size() < 2) return NAN;
    uint16_t val = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
    return static_cast<float>(val) / 10.0f;
  }

  if (type_hint == "INT8_NUMBER" || type_hint == "UINT8_NUMBER") {
    if (bytes.empty()) return NAN;
    return static_cast<float>(bytes[0]);
  }

  // For history / arrays etc we don't expose as floats directly
  return NAN;
}

bool DometicCfxBle::decode_to_bool_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  if (bytes.empty()) return false;
  if (type_hint == "INT8_BOOLEAN") {
    return bytes[0] != 0;
  }
  // fallback: anything nonzero = true
  return bytes[0] != 0;
}

std::string DometicCfxBle::decode_to_string_(const std::vector<uint8_t> &bytes, const std::string &type_hint) {
  if (type_hint == "UTF8_STRING") {
    if (bytes.empty()) return "";
    size_t end = 0;
    while (end < bytes.size() && end < 15 && bytes[end] != 0x00) {
      end++;
    }
    return std::string(reinterpret_cast<const char *>(bytes.data()), end);
  }

  // For history / arrays just hex-dump as a fallback
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
  if (type_hint == "INT8_BOOLEAN" || type_hint == "INT8_NUMBER" || type_hint == "UINT8_NUMBER") {
    out.push_back(static_cast<uint8_t>(value ? 1 : 0));
    return out;
  }
  out.push_back(static_cast<uint8_t>(value ? 1 : 0));
  return out;
}

std::vector<uint8_t> DometicCfxBle::encode_from_float_(float value, const std::string &type_hint) {
  std::vector<uint8_t> out;

  if (type_hint == "INT16_DECIDEGREE_CELSIUS") {
    int16_t deci = static_cast<int16_t>(std::lround(value * 10.0f));
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

  // Fallback: 1 byte
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
    if (v == NO_VALUE) {
      return desc + " is unavailable";
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %.1f°C", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "INT8_BOOLEAN") {
    bool v = decode_to_bool_(bytes, type);
    std::string lower = desc;
    for (auto &c : lower) c = static_cast<char>(tolower(c));
    const char *state_str = nullptr;
    if (lower.find("power") != std::string::npos) {
      state_str = v ? "on" : "off";
    } else {
      state_str = v ? "active" : "inactive";
    }
    return desc + " is " + state_str;
  }

  if (type == "UINT8_NUMBER" && topic_key == "BATTERY_PROTECTION_LEVEL") {
    if (bytes.empty()) return "";
    int v = bytes[0];
    const char *label = battery_level_str(v);
    if (label != nullptr) {
      return desc + " is " + label;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s is %d", desc.c_str(), v);
    return std::string(buf);
  }

  if (type == "INT8_NUMBER" && topic_key == "POWER_SOURCE") {
    if (bytes.empty()) return "";
    int v = bytes[0];
    const char *label = power_source_str(v);
    if (label != nullptr) {
      return desc + " is " + label;
    }
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
    snprintf(buf, sizeof(buf), "%s is %.1f to %.1f°C", desc.c_str(), min_v, max_v);
    return std::string(buf);
  }

  if (type == "HISTORY_DATA_ARRAY") {
    if (bytes.size() < 15) return "";
    char buf[256];
    std::string line = desc + ": temps [";
    for (int i = 0; i < 7; i++) {
      std::vector<uint8_t> b(bytes.begin() + i * 2, bytes.begin() + i * 2 + 2);
      float t = decode_to_float_(b, "INT16_DECIDEGREE_CELSIUS");
      snprintf(buf, sizeof(buf), "%.1f°C", t);
      if (i != 0) line += ", ";
      line += buf;
    }
    uint8_t ts = bytes[14];
    snprintf(buf, sizeof(buf), "], timestamp %u", static_cast<unsigned>(ts));
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

// ----------------- Entity helpers -------------------------------------------

void DometicCfxBleSwitch::write_state(bool state) {
  if (parent_ == nullptr) {
    ESP_LOGW(TAG, "Switch write_state without parent");
    publish_state(state);
    return;
  }

  parent_->send_switch(topic_, state);
  publish_state(state);
}

void DometicCfxBleNumber::control(float value) {
  if (parent_ == nullptr) {
    ESP_LOGW(TAG, "Number control without parent");
    publish_state(value);
    return;
  }

  parent_->send_number(topic_, value);
  publish_state(value);
}

}  // namespace dometic_cfx_ble
}  // namespace esphome
