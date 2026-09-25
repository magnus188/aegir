# Convenience entry point for the ESP32-P4 firmware workspace.
.DEFAULT_GOAL := help

FIRMWARE_DIR := software/firmware

.PHONY: help setup-idf doctor devices board-info configure menuconfig build build-all \
	firmware push upload flash push-monitor monitor size size-check test check \
	sim-deps sim-configure sim-build sim emulator run sim-test clean clean-sim clean-all

help setup-idf doctor devices board-info configure menuconfig build build-all \
firmware push upload flash push-monitor monitor size size-check test check \
sim-deps sim-configure sim-build sim emulator run sim-test clean clean-sim clean-all:
	@$(MAKE) -C $(FIRMWARE_DIR) $@
