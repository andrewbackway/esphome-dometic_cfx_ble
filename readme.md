# ESPHome Dometic CFX BLE Component

Custom ESPHome component to control and monitor **Dometic CFX** fridges over **Bluetooth Low Energy** using the native ESP-IDF BLE stack. It implements the Dometic pub/sub protocol and exposes it as standard ESPHome entities.

- Supports **single-zone (SZ)**, **single-zone + icemaker (SZI)**, and **dual-zone (DZ)** models via `product_type`.
- Reads live data: compartment temperatures, door state, battery voltage, alerts, etc.
- Controls: overall power, per-compartment power, set temperatures, and more.

👉 **Quick links:**

- Example config: [`example.yaml`](example.yaml)  
- Protocol reference: [`protocol.md`](protocol.md)  

---

## Requirements

- ESPHome (ESP32 / ESP-IDF target)
- ESP32 (tested on ESP32-S3)
- BLE enabled with:
  - `esp32_ble_tracker`
  - `ble_client` (used via `BLEClientNode`)

---

## Installation

In your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/<your-username>/esphome-dometic-cfx-ble
    components: [dometic_cfx_ble]
```

## Basic Configuration

See the full example in [`example.yaml`](example.yaml), but the core pieces are:

```yaml
esp32:
  board: esp32-s3-dev
  framework:
    type: esp-idf

external_components:
  - source:
      type: git
      url: https://github.com/<your-username>/esphome-dometic-cfx-ble
    components: [dometic_cfx_ble]

esp32_ble_tracker:

ble_client:
  - mac_address: AA:BB:CC:DD:EE:FF
    id: cfx_ble_client

dometic_cfx_ble:
  id: dometic_cfx_ble1
  ble_client_id: cfx_ble_client
  product_type: DZ  # SZ = Single Zone | SZI = Single Zone Icemaker | DZ = Dual Zone 
```

Then add entities bound to protocol “topics” (see example for more):

```yaml
sensor:
  - platform: dometic_cfx_ble
    dometic_cfx_ble_id: dometic_cfx_ble1
    type: COMPARTMENT_0_MEASURED_TEMPERATURE
    name: "CFX Zone 1 Temp"
    unit_of_measurement: "°C"

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
    unit_of_measurement: "°C"
    min_value: -22
    max_value: 10
    step: 1
```

Each `type` corresponds to a protocol topic (see `TOPIC_TYPES` in `__init__.py` and the topic list in `protocol.md`).

---

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
