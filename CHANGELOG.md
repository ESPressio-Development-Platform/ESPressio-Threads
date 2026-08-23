# Changelog

## 3.1.6 — 2026-08-23

### Changed
- Raised required ESPressio Timing from `>=2.2.5 <3.0.0` to `>=2.2.7 <3.0.0`, propagating the corrected Units 0.2.6 / Serializable 0.11.2 generation downstream.
- Updated package metadata and CI validation for the corrected 3.1.6 cascade generation.
- Added explicit ESP32 compile validation of the opt-in `ESPressio_PrecisionThread_Serializable.hpp` surface with Timing 2.2.7, Units 0.2.6 and Serializable 0.11.2.
- Preserved the core dependency boundary: Threads depends directly on Timing and Observable only; Serializable support remains opt-in through Serializable Unit time/frequency representations.

### Compatibility
- Threads retains no direct Serializable dependency; Serializable PrecisionThread representations remain opt-in.
- No public Threads API or runtime behaviour changes are introduced by this dependency-maintenance release.

## 3.1.5 — 2026-08-22

### Changed
- Published the post-migration ESPressio Threads package generation from `ESPressio-Development-Platform`.
- Raised required ESPressio Timing from `>=2.2.4 <3.0.0` to `>=2.2.5 <3.0.0`.
- Raised required ESPressio Observable from `>=3.0.1 <4.0.0` to `>=3.0.2 <4.0.0`.
- Updated package metadata, README installation/dependency guidance, CI validation, and dependency documentation.

### Compatibility
- No Threads public API or runtime behaviour changes are introduced by this repository-relocation patch release.

All notable changes to this project are documented in this file.

The structure follows the principles of [Keep a
Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic
Versioning](https://semver.org/).

> **Historical note:** This changelog was reconstructed retrospectively
> from published GitHub Releases, tags, release notes, repository
> history, and the documented public API. Where an historical release
> had little or no release-note detail, the entry is intentionally terse
> rather than inferring unsupported intent.

## 3.1.4 — 2026-08-21

### Changed
- Raised required ESPressio Timing from `>=2.2.3 <3.0.0` to `>=2.2.4 <3.0.0` following the coordinated dependency refresh.
- Preserved ESPressio Observable at `>=3.0.1 <4.0.0`.
- Updated package metadata and dependency documentation.

### Compatibility
- No Threads public API or runtime behaviour changes are introduced by this dependency-maintenance release.

## 3.1.3 — 2026-08-20

### Changed
- Raised required ESPressio Timing from `>=2.2.2 <3.0.0` to `>=2.2.3 <3.0.0`.
- Preserved ESPressio Observable at `>=3.0.1 <4.0.0`.
- Updated dependency documentation and validation.

### Compatibility
- No Threads public API or runtime behaviour changes are introduced by this dependency-maintenance release.

## 3.1.2 — 2026-08-20

### Changed
- Raised the required ESPressio Timing baseline from 2.2.1 to 2.2.2.
- Raised the required ESPressio Observable baseline from 3.0.0 to 3.0.1.
- Preserved the Threads 3.1.x public API and behaviour.

## 3.1.1 — 2026-08-19

### Changed
- Updated required ESPressio dependency baselines to the latest compatible released versions.
- Bounded dependency compatibility to the current major versions.

## 3.1.0

### Added
- Added `PrecisionThread<TTime, TRepresentationTraits>` with generic typed-time representations.
- Added `PrecisionThreadTraits<TTime>` and opt-in Serializable representations.
- Added Observer integration for Thread lifecycle and PrecisionThread scheduling/infrastructure notifications.

### Changed
- Preserved ordinary PrecisionThread use without requiring Serializable.

## 3.0.0

### Changed
- Major Threads architecture update introducing the current Thread lifecycle, manager, termination-dispatch and generic PrecisionThread generation.
