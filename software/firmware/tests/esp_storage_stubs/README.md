# ESP storage adapter test declarations

This directory supplies only low-level ESP/NVS declarations for `test_esp_storage_adapter`. Compile the actual `storage_service.cpp` with `ESP_PLATFORM=1` and without `TRIMIX_SIMULATOR`. Keep this include directory before other ESP stubs. Each test scenario is a separate process because the production initializer is intentionally a singleton.

The test implements a map-backed NVS/OTA adapter. It exercises real production initialization, probation gating, namespace preservation, capacity checks and journal calls. Failure cases model both no persistence and persistence followed by a lost commit acknowledgement. They do not simulate Espressif NVS page internals, torn flash cells, garbage collection, bootloader execution, power cuts or endurance. No flash erase symbol is supplied, so introducing an automatic partition erase into the tested production branch also fails linkage.
