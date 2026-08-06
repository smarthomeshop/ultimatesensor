# UltimateSensor V2 ESPHome Firmware

UltimateSensor V2 is sold as standard hardware with LD2412 + LD2450, plus PIR, microphone, speaker, environmental sensors, and WiFi/Ethernet firmware variants. The optional LD2460 is an upgrade module: customers remove/replace the LD2450 module, install the LD2460 module, and flash one of the `*-ld2460.yaml` firmware variants.

Leave the LD2412 installed. LD2412 and PIR remain reliable presence fallbacks and make occupancy more robust together with either LD2450 or LD2460.

## Package Layout

| Package | Purpose |
| --- | --- |
| `base.yaml` | Shared ESP32-S3 hardware, sensors, PIR, LD2412, microphone, speaker, media player, API, OTA, and combined occupancy logic |
| `tracking-ld2450.yaml` | Product pin wrapper for shared LD2450 tracking and Room Designer zones on GPIO11/GPIO12 |
| `tracking-ld2460.yaml` | Product pin wrapper for shared LD2460 tracking and Room Designer zones on GPIO6/GPIO5 |
| `wifi.yaml` | WiFi firmware network stack, captive portal, Improv, WiFi diagnostics, Ethernet power off |
| `ethernet.yaml` | W5500 Ethernet firmware network stack and Ethernet diagnostics |
| `voice-assistant.yaml` | Local wake word and Home Assistant voice assistant package |
| `cloud-voice.yaml` | Local wake word and SmartHomeShop Cloud Voice package for cloud firmware |
| `complete.yaml` | SPS30 particulate matter sensor and PM idle controls |
| `led-lighting.yaml` | Persistent local controls for motion light, night light, and CO2 warning flashes |

## Firmware Variants

Standard variants are for the product as shipped with LD2412 + LD2450.

| File | Network | SPS30 | Tracking radar | Voice service |
| --- | --- | --- | --- | --- |
| `ultimatesensor-v2-wifi-basic.yaml` | WiFi | No | LD2450 | Home Assistant |
| `ultimatesensor-v2-wifi-complete.yaml` | WiFi | Yes | LD2450 | Home Assistant |
| `ultimatesensor-v2-ethernet-basic.yaml` | Ethernet W5500 | No | LD2450 | Home Assistant |
| `ultimatesensor-v2-ethernet-complete.yaml` | Ethernet W5500 | Yes | LD2450 | Home Assistant |
| `ultimatesensor-v2-wifi-basic-cloud.yaml` | WiFi | No | LD2450 | SmartHomeShop Cloud |
| `ultimatesensor-v2-wifi-complete-cloud.yaml` | WiFi | Yes | LD2450 | SmartHomeShop Cloud |
| `ultimatesensor-v2-ethernet-basic-cloud.yaml` | Ethernet W5500 | No | LD2450 | SmartHomeShop Cloud |
| `ultimatesensor-v2-ethernet-complete-cloud.yaml` | Ethernet W5500 | Yes | LD2450 | SmartHomeShop Cloud |

LD2460 upgrade variants are only for devices where the LD2450 module has been removed/replaced by the optional LD2460 module.

Read [LD2460 Upgrade and Calibration](LD2460-UPGRADE.md) before mounting the
optional module. Module orientation, wall mode, installation height and angle
all affect the reported coordinates.

| File | Network | SPS30 | Tracking radar | Voice service |
| --- | --- | --- | --- | --- |
| `ultimatesensor-v2-wifi-basic-ld2460.yaml` | WiFi | No | LD2460 | Home Assistant |
| `ultimatesensor-v2-wifi-complete-ld2460.yaml` | WiFi | Yes | LD2460 | Home Assistant |
| `ultimatesensor-v2-ethernet-basic-ld2460.yaml` | Ethernet W5500 | No | LD2460 | Home Assistant |
| `ultimatesensor-v2-ethernet-complete-ld2460.yaml` | Ethernet W5500 | Yes | LD2460 | Home Assistant |
| `ultimatesensor-v2-wifi-basic-ld2460-cloud.yaml` | WiFi | No | LD2460 | SmartHomeShop Cloud |
| `ultimatesensor-v2-wifi-complete-ld2460-cloud.yaml` | WiFi | Yes | LD2460 | SmartHomeShop Cloud |
| `ultimatesensor-v2-ethernet-basic-ld2460-cloud.yaml` | Ethernet W5500 | No | LD2460 | SmartHomeShop Cloud |
| `ultimatesensor-v2-ethernet-complete-ld2460-cloud.yaml` | Ethernet W5500 | Yes | LD2460 | SmartHomeShop Cloud |

ESPHome Ethernet and WiFi are kept as separate firmware variants. The WiFi variants keep the W5500 powered off. The Ethernet variants power the W5500 and expose Ethernet network info sensors.

## Radar Policy

- Factory default: keep LD2412 installed and use LD2450 firmware.
- LD2460 upgrade: remove/replace the LD2450 module, install LD2460, and flash a `*-ld2460.yaml` firmware file.
- Do not remove LD2412 for normal installs. It is the reliable close-range/still-presence fallback.
- `Occupancy` is always `PIR motion OR LD2412 presence OR tracking radar presence`.
- The standard LD2450 firmware exposes Bluetooth/configuration controls for the LD2450.
- LD2450 tracking and zones are maintained centrally in `smarthomeshop/ld2450`.
- LD2460 tracking and zones are maintained centrally in `smarthomeshop/ld2460`.
- These local tracking files only select the correct product pins and shared packages.
- LD2460 firmware verifies `side` installation mode after every boot and reads
  the saved installation height and angle back from the radar.

## Pin Map

| Function | GPIO |
| --- | --- |
| I2C SDA | GPIO10 |
| I2C SCL | GPIO9 |
| PIR motion | GPIO1 |
| LD2412 TX/RX | GPIO14 / GPIO13 |
| LD2450 TX/RX, standard hardware | GPIO11 / GPIO12 |
| LD2460 TX/RX, optional upgrade | GPIO6 / GPIO5 |
| Microphone WS/SD/BCLK | GPIO7 / GPIO15 / GPIO16 |
| Speaker LRCLK/DOUT/BCLK | GPIO8 / GPIO17 / GPIO18 |
| Front RGB LEDs | GPIO43, 3x WS2812/GRB |
| ESP status LED | GPIO44 |
| Native USB D- / D+ | GPIO19 / GPIO20 |
| W5500 power enable | GPIO47, inverted |
| W5500 CLK | GPIO21 |
| W5500 MOSI | GPIO45 |
| W5500 MISO | GPIO38 |
| W5500 CS | GPIO41 |
| W5500 interrupt | GPIO39 |
| W5500 reset | GPIO40 |

## Notes

- Production V2 boards use the ESP32-S3 native USB Serial/JTAG interface for flashing and logs.
- Basic variants omit `complete.yaml`.
- Complete variants include `complete.yaml`, which adds the SPS30 at I2C address `0x69`.
- Voice assistant support is part of every UltimateSensor V2 variant.
- The local wake phrase is `Okay Nabu` and can be toggled with
  `Enable Voice Assistant`.
- Standard firmware sends voice requests to Home Assistant. Cloud firmware uses
  SmartHomeShop Cloud Voice and does not require Home Assistant. Claim the
  device in the SmartHomeShop App and enable Cloud Voice before first use.
- WiFi variants include Improv, a fallback hotspot, and the branded
  SmartHomeShop captive setup portal. Ethernet variants use wired networking
  and therefore do not create a WiFi setup hotspot.
- Automatic LED lighting defaults to off. Local firmware exposes persistent
  Home Assistant controls; cloud firmware receives the same settings from the
  SmartHomeShop App.
- Voice Assistant and boot LED animations take priority over automatic motion,
  night-light, and CO2 indications.
