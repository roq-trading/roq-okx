# Change Log

All notable changes will be documented in this file.

## Head

### Fixed

* Only disconnect the public WS connections when the `service_type` is WS (#615)
* The notice event was not being parsed (#614)

## 1.1.6 &ndash; 2026-07-08

### Removed

* BREAKING CHANGE: Remove default URIs (#611)

## 1.1.5 &ndash; 2026-06-06

### Added

* Support `pxAmendType` (#595)

### Fixed

* Incorrect mapping of `time_in_force=IOC` (#587)

## 1.1.4 &ndash; 2026-04-20

### Fixed

* Placing order failed without instIdCode (#575)

## 1.1.3 &ndash; 2026-03-12

### Fixed

* `OrderUpdate.remaining_quantity` not populated correctly (#568)

## 1.1.2 &ndash; 2026-02-08

### Fixed

* Quantity and price was not not formatted with correct decimals (#541)

### Changed

* Separate subscriptions for (OKX's) system status and instrument definitions from regular market data (#543)

## 1.1.1 &ndash; 2025-12-14

## 1.1.0 &ndash; 2025-11-22

## 1.0.9 &ndash; 2025-09-26

### Fixed

* HTTP response with status code 503 (service unavailable) should map to `RequestStatus::REJECTED` (#522)

## 1.0.8 &ndash; 2025-08-16

## 1.0.7 &ndash; 2025-07-02

### Fixed

* Some subscriptions were incorrectly removed for futures and swaps (#511)

## 1.0.6 &ndash; 2025-05-16

## 1.0.5 &ndash; 2025-03-26

## 1.0.4 &ndash; 2024-12-30

## 1.0.3 &ndash; 2024-11-26

### Fixed

*  Incorrect market data subscription logic (#467)
*  Order book processing didn't check previous sequence id (#466)

## 1.0.2 &ndash; 2024-07-14

### Fixed

* MbP `number_of_orders` exceeded 65k (#461)

## 1.0.1 &ndash; 2024-04-14

## 1.0.0 &ndash; 2024-03-16

## 0.9.9 &ndash; 2024-01-28

## 0.9.8 &ndash; 2023-11-20

## 0.9.7 &ndash; 2023-09-18

## 0.9.6 &ndash; 2023-07-22

## 0.9.5 &ndash; 2023-06-12

## 0.9.4 &ndash; 2023-05-04

## 0.9.3 &ndash; 2023-03-20

## 0.9.2 &ndash; 2023-02-22

## 0.9.1 &ndash; 2023-01-12

## 0.9.0 &ndash; 2022-12-22

## 0.8.9 &ndash; 2022-11-14

## 0.8.8 &ndash; 2022-10-04

## 0.8.7 &ndash; 2022-08-22

## 0.8.6 &ndash; 2022-07-18

### Fixed

* Using `--ws_books_depth=5` was not working (#239)

## 0.8.5 &ndash; 2022-06-06

### Changed

* `TopOfBook` now sourced from `bbo-tbt` (#231)
* Market data support for `--net_disconnect_on_idle_timeout`.

### Fixed

* Market data subscription was not working correctly after disconnect (#226)

## 0.8.4 &ndash; 2022-05-14

### Added

* New flag `--ws_disconnect_timeout` (#218)

### Fixed

* Order book subscription now depends on client tiering (#212)

## 0.8.3 &ndash; 2022-03-22

## 0.8.2 &ndash; 2022-02-18

### Changed

* Rename to OKX (from OKEx) (#164)
* New URLs / end-points (#163)

### Added

* Add funding rate and index price (#161)

## 0.8.1 &ndash; 2022-01-16

## 0.8.0 &ndash; 2022-01-12
