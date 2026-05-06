# Changelog

All notable customer-facing firmware changes for UltimateSensor are tracked in this file.

This changelog starts on 2026-05-06. Earlier firmware versions existed before that date, but they were not tracked in a customer-facing changelog.

## [Unreleased]

- Add customer-facing firmware notes here before merging a PR.

## [UltimateSensor V1 1.5] - 2026-05-06


- Added automated firmware builds, GitHub Pages firmware manifests, GitHub Releases, and changelog validation for future releases.
- Reworked the firmware repository into shared ESPHome packages so the WiFi/Ethernet and Basic/Complete variants stay easier to maintain consistently.
- CO2 ambient pressure compensation and manual calibration controls are now part of the shared base firmware for all variants.
- Aligned Complete variant SPS30 measurement behavior with UltimateSensor Mini, including 60-second default updates, SPS30 idle-cycle controls, and matching PM sensor IDs.


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
