# ESPHome Dometic CFX BLE Component

[![ESPHome](https://img.shields.io/badge/ESPHome-2026.04.0%2B-blue.svg)](https://esphome.io/)
[![ESP32](https://img.shields.io/badge/ESP32-ESP--IDF-E7352C.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/andrewbackway/esphome-dometic_cfx_ble)
[![CI](https://github.com/andrewbackway/esphome-dometic_cfx_ble/actions/workflows/ci.yml/badge.svg)](https://github.com/andrewbackway/esphome-dometic_cfx_ble/actions/workflows/ci.yml)

 ESPHome External Component to control and monitor **Dometic CFX** fridges over **Bluetooth Low Energy** using the native ESP-IDF BLE stack. It implements the Dometic pub/sub protocol and exposes it as standard ESPHome entities.

- Supports **single-zone (SZ)**, **single-zone + icemaker (SZI)**, and **dual-zone (DZ)** models via `product_type`.
- Reads live data: compartment temperatures, door state, battery voltage, alerts, etc.
- Controls: overall power, per-compartment power, set temperatures, and more.

**Quick links:**

- Example config: [`example.yaml`](example.yaml)  
- Protocol reference: [`protocol.md`](protocol.md)  

---

## Requirements

- ESPHome **2026.04.0 or newer** (ESP32 / ESP-IDF target)
- ESP32 (tested on ESP32-S3)
- BLE enabled with:
  - `esp32_ble_tracker`
  - `ble_client` (used via `BLEClientNode`)

---

## Installation

### Finding your cooler's MAC address

You'll need the BLE MAC address of your CFX unit. There are three ways to get it:

1. **Pair your phone with the cooler.** On the Device Details page, the device's
   Bluetooth address should be listed.
2. **Use nRF Connect** on your mobile device: list nearby devices and look for one named
   `CFX_XXXXX`. The address shown under the name is the MAC address.
3. **Have your ESP device log nearby BLE devices.** Add the following to your ESPHome YAML:

   ```yaml
   
  logger:
    level: DEBUG

   esp32_ble_tracker:
     on_ble_advertise:
       then:
         - lambda: |-
             ESP_LOGD("ble_adv", "New BLE device");
             ESP_LOGD("ble_adv", "  address: %s", x.address_str().c_str());
             ESP_LOGD("ble_adv", "  name: %s", x.get_name().c_str());
   ```

   After installing that on your device, filter the logs for your cooler:

   ```bash
   esphome logs config.yaml | grep -C 2 -i cfx3
   ```

   (On Windows PowerShell, use `Select-String -Context 2 -Pattern cfx3` instead of `grep`.)

   Thanks to [@WanderingStar](https://github.com/WanderingStar) for this suggestion!

### Add the component

In your ESPHome YAML:

```yaml
external_components:
  - source: github://andrewbackway/esphome-dometic_cfx_ble@main
```

## Basic Configuration

See the full example in [`example.yaml`](example.yaml), but the core pieces are:

```yaml
esp32:
  board: esp32-s3-dev
  framework:
    type: esp-idf

external_components:
  - source: github://andrewbackway/esphome-dometic_cfx_ble@main

esp32_ble_tracker:

ble_client:
  - mac_address: AA:BB:CC:DD:EE:FF
    id: cfx_ble_client

dometic_cfx_ble:
  id: dometic_cfx_ble1
  ble_client_id: cfx_ble_client
  product_type: DZ  # SZ = Single Zone | SZI = Single Zone Icemaker | DZ = Dual Zone 
  temperature_unit: C # Optional, defaults to Celsius. Set to F for Fahrenheit.
```

Then add entities bound to protocol “topics” (see example for more):

```yaml
sensor:
  - platform: dometic_cfx_ble
    dometic_cfx_ble_id: dometic_cfx_ble1
    type: COMPARTMENT_0_MEASURED_TEMPERATURE
    name: "CFX Zone 1 Temp"

switch:
  - platform: dometic_cfx_ble
    dometic_cfx_ble_id: dometic_cfx_ble1
    type: COOLER_POWER
    name: "CFX Power"

number:
  - platform: dometic_cfx_ble
    dometic_cfx_ble_id: dometic_cfx_ble1
    type: COMPARTMENT_0_SET_TEMPERATURE
    name: "CFX Zone 1 Set Temp"
    min_value: -22
    max_value: 10
    step: 1
```

Each `type` corresponds to a protocol topic (see `TOPIC_TYPES` in `__init__.py` and the topic list in `protocol.md`).

---

## Temperature Units

Set `temperature_unit` on the main component to `C` (default) or `F`:

```yaml
dometic_cfx_ble:
  id: dometic_cfx_ble1
  ble_client_id: cfx_ble_client
  product_type: DZ
  temperature_unit: F  # or C (default)
```

- The component tells the fridge which unit to display via the `PRESENTED_TEMPERATURE_UNIT` topic on connect.
- All `COMPARTMENT_*_MEASURED_TEMPERATURE` sensors automatically report in the chosen unit and the `unit_of_measurement` is set to `°C` or `°F` accordingly — no manual override needed.
- For `COMPARTMENT_*_SET_TEMPERATURE` number entities, specify `min_value`/`max_value` in the chosen unit. The component converts values to the device's internal format automatically.
  - Celsius typical range: `min_value: -22`, `max_value: 10`
  - Fahrenheit typical range: `min_value: -8`, `max_value: 50`

---

Set your fridge Bluetooth Mode to PAIR, and run the module.

test.py is also available to validate operation locally if needed (pair is required).

## How It Works

- Connects to the Dometic CFX BLE service (`537a0300-0995-481f-926c-1604e23fd515`) and uses:
  - a write characteristic for commands
  - a notify characteristic for PUB updates
- Implements the Dometic `PUB` / `SUB` / `ACK` / `NAK` / `PING` frame format and maintains a small send queue.
- Maps topics to ESPHome entities (`sensor`, `binary_sensor`, `switch`, `number`, `text_sensor`) and converts protocol types (e.g. `INT16_DECIDEGREE_CELSIUS`) into usable values.

For all protocol details, value encodings, and topic list, read [`protocol.md`](protocol.md).

---

## License

MIT
