# Changelog

All notable customer-facing firmware changes for UltimateSensor are tracked in this file.

This changelog starts on 2026-05-06. Earlier firmware versions existed before that date, but they were not tracked in a customer-facing changelog.

## [Unreleased]

- Reworked UltimateSensor V1 and V2 LD2450 tracking to use the native ESPHome LD2450 component plus SmartHomeShop polygon zones, entry lines, people counting, and Home Assistant zone push actions.

## [UltimateSensor V1 1.10] - 2026-07-09


- Updated UltimateSensor V1 LAN8720 Ethernet clock configuration for ESPHome 2026.7 compatibility.
- Added faster cloud presence updates for occupancy and zone counts.
- Replaced the cloud firmware status LED behavior so cloud-only devices blink while offline and stay off once online.


## [UltimateSensor V2 1.2] - 2026-07-09


- Added UltimateSensor V2 boot sound and a three-LED left-to-right boot animation.
- Added faster cloud presence updates for occupancy and zone counts.
- Replaced the cloud firmware status LED behavior so cloud-only devices blink while offline and stay off once online.


## [UltimateSensor V1 1.9] - 2026-06-04


- Added SmartHomeShop App cloud firmware variants for UltimateSensor V1 WiFi and Ethernet Basic/Complete firmware.
- Added cloud payload IDs for VOC, NOx, and occupancy.
- Fixed GitHub Release note extraction so dated changelog headings publish the matching customer-facing notes.


## [UltimateSensor V2 1.1] - 2026-06-04


- Added SmartHomeShop App cloud firmware variants for UltimateSensor V2 WiFi, Ethernet, LD2450, and optional LD2460 firmware families.
- Added LD2450 zone target count IDs for V2 cloud zone sync.
- Fixed GitHub Release note extraction so dated changelog headings publish the matching customer-facing notes.


## [UltimateSensor V2 1.0] - 2026-05-31


- Added UltimateSensor V2 ESP32-S3 firmware variants for WiFi and W5500 Ethernet, with LD2412, LD2450, PIR, I2S microphone, speaker media player, and optional SPS30 particulate matter support.
- Split UltimateSensor V2 tracking radar support into standard LD2450 packages and optional LD2460 upgrade firmware variants.
- Documented that LD2412 should remain installed when customers replace the LD2450 module with the optional LD2460 upgrade.
- Added local wake-word and Home Assistant voice assistant support to UltimateSensor V2 firmware.


## [UltimateSensor V1 1.8] - 2026-05-06

### Added

- Added native LD2450 Bluetooth, restart, and factory-reset controls while preserving the existing target and zone entities.


## [UltimateSensor V1 1.7] - 2026-05-06

### Changed

- Aligned Complete variant SPS30 measurement behavior with UltimateSensor Mini, including 60-second default updates, SPS30 idle-cycle controls, and matching PM sensor IDs.

## [UltimateSensor V1 1.6] - 2026-05-06

### Changed

- CO2 ambient pressure compensation and manual calibration controls are now part of the shared base firmware for all variants.

## [UltimateSensor V1 1.5] - 2026-05-06

### Added

- Added automated firmware builds, GitHub Pages firmware manifests, GitHub Releases, and changelog validation for future releases.

### Changed

- Reworked the firmware repository into shared ESPHome packages so the WiFi/Ethernet and Basic/Complete variants stay easier to maintain consistently.

## [UltimateSensor V1 1.4] - 2026-05-06

### Added

- Customer-facing version history now lives in this repository.
- GitHub Releases will now be published for future firmware versions.
- Added a Home Assistant **LCD Display** switch to turn the OLED display on or off.
- Added a Home Assistant **PM Sensor** switch for Complete firmware variants.
- Added **Night Mode** for Complete firmware variants, turning off the OLED display and PM sensor together.

### Fixed

- PM sensor shutdown now uses the correct SPS30 stop-measurement action.

### Changed

- This file is now the public place for customer-facing firmware notes.
