# Ægir developer commands

SHELL := /bin/bash
.DEFAULT_GOAL := help

TARGET := esp32p4
PORT ?=
P4_REV ?= auto
FLASH_BAUD ?= 460800
ZOOM ?= 1.0
BUILD_TYPE ?= Debug

ESP_IDF_VERSION := v5.5.4
ESP_IDF_DIR ?= $(HOME)/esp/$(ESP_IDF_VERSION)/esp-idf
IDF_EXPORT := $(ESP_IDF_DIR)/export.sh
ESP_PYTHON ?= $(shell command -v python3.13 2>/dev/null)
ESP_PYTHON_BIN_DIR ?= $(shell if [ -n "$(ESP_PYTHON)" ]; then "$(ESP_PYTHON)" -c 'import os, sys; print(os.path.join(os.path.dirname(os.path.dirname(sys.executable)), "libexec", "bin"))'; fi)
IDF_PYTHON_ENV ?= $(HOME)/.espressif/python_env/idf5.5_py3.13_env

FIRMWARE_BUILD_ROOT ?= build
SIM_BUILD_DIR ?= simulator/build
COMMON_DEFAULTS := sdkconfig.defaults;sdkconfig.defaults.esp32p4

PORT_ARG = $(if $(strip $(PORT)),-p "$(PORT)",)
FLASH_ARGS = $(PORT_ARG) $(if $(strip $(FLASH_BAUD)),-b "$(FLASH_BAUD)",)

define activate_idf
	if [ ! -f "$(IDF_EXPORT)" ]; then \
		printf 'ESP-IDF $(ESP_IDF_VERSION) is missing at %s. Run make setup-idf.\n' "$(ESP_IDF_DIR)" >&2; \
		exit 1; \
	fi; \
	if [ -n "$(ESP_PYTHON)" ]; then export PATH="$(ESP_PYTHON_BIN_DIR):$$PATH"; fi; \
	export IDF_PYTHON_ENV_PATH="$(IDF_PYTHON_ENV)"; \
	. "$(IDF_EXPORT)" >/dev/null
endef

define resolve_revision
	revision="$(P4_REV)"; \
	if [ "$$revision" = "auto" ]; then \
		if [ -n "$(PORT)" ]; then \
			revision="$$(./scripts/detect_p4_revision.sh "$(PORT)")"; \
		else \
			revision="pre3"; \
			printf 'No PORT supplied; building the Guition pre-v3 preset. Use P4_REV=v3 for production silicon.\n'; \
		fi; \
	fi; \
	case "$$revision" in \
		pre3|v3) ;; \
		*) printf 'P4_REV must be auto, pre3, or v3 (got %s).\n' "$$revision" >&2; exit 2 ;; \
	esac; \
	build_dir="$(abspath $(FIRMWARE_BUILD_ROOT))/esp32p4-$$revision"; \
	defaults="$(COMMON_DEFAULTS);sdkconfig.defaults.esp32p4.$$revision"; \
	if [ "$$revision" = "pre3" ]; then \
		partitions="$(abspath partitions.esp32p4.pre3.csv)"; \
	else \
		partitions="$(abspath partitions.csv)"; \
	fi
endef

define run_idf
	@set -euo pipefail; \
	$(activate_idf); \
	$(resolve_revision); \
	printf 'ESP32-P4 profile: %s\nBuild directory: %s\n' "$$revision" "$$build_dir"; \
	idf.py -B "$$build_dir" \
		-D "IDF_TARGET=$(TARGET)" \
		-D "SDKCONFIG=$$build_dir/sdkconfig" \
		-D "SDKCONFIG_DEFAULTS=$$defaults" $(1)
endef

.PHONY: help setup-idf doctor devices board-info configure menuconfig build build-all firmware \
	push upload flash push-monitor monitor size size-check test check sim-deps sim-configure \
	sim-build sim emulator run sim-test clean clean-sim clean-all

help: ## Show available commands
	@printf 'Ægir — ESP32-P4 native portrait firmware\n\n'
	@printf 'Usage: make <target> [PORT=/dev/...] [P4_REV=auto|pre3|v3] [ZOOM=0.75]\n\n'
	@awk 'BEGIN { FS = ":.*##" } /^[a-zA-Z0-9_-]+:.*##/ { printf "  %-18s %s\n", $$1, $$2 }' $(MAKEFILE_LIST)
	@printf '\nExamples:\n'
	@printf '  make setup-idf\n'
	@printf '  make board-info PORT=/dev/cu.usbserial-110\n'
	@printf '  make push PORT=/dev/cu.usbserial-110\n'
	@printf '  make sim ZOOM=0.75\n'

setup-idf: ## Install the pinned ESP-IDF and its ESP32-P4 tools
	@set -euo pipefail; \
	if [ -z "$(ESP_PYTHON)" ]; then \
		printf 'Python 3.13 is required. Install it with: brew install python@3.13\n' >&2; \
		exit 1; \
	fi; \
	if [ ! -d "$(ESP_IDF_DIR)/.git" ]; then \
		mkdir -p "$$(dirname "$(ESP_IDF_DIR)")"; \
		git clone --branch "$(ESP_IDF_VERSION)" --recursive --depth 1 \
			https://github.com/espressif/esp-idf.git "$(ESP_IDF_DIR)"; \
	fi; \
	PATH="$(ESP_PYTHON_BIN_DIR):$$PATH" \
		IDF_PYTHON_ENV_PATH="$(IDF_PYTHON_ENV)" \
		"$(ESP_IDF_DIR)/install.sh" esp32p4; \
	printf 'ESP-IDF $(ESP_IDF_VERSION) is ready.\n'

doctor: ## Check the pinned firmware and simulator prerequisites
	@set -euo pipefail; \
	printf 'Python:  '; \
	if [ -n "$(ESP_PYTHON)" ]; then "$(ESP_PYTHON)" --version; else printf 'missing Python 3.13\n'; exit 1; fi; \
	$(activate_idf); \
	printf 'ESP-IDF: '; idf.py --version; \
	idf.py --version | grep -q 'v5.5.4' || { printf 'Expected ESP-IDF v5.5.4.\n' >&2; exit 1; }; \
	printf 'CMake:   '; cmake --version | sed -n '1p'; \
	printf 'C++:     '; "$(CXX)" --version | sed -n '1p'; \
	printf 'SDL2:    '; pkg-config --modversion sdl2

devices: ## List likely ESP32 serial ports
	@ports="$$(find /dev -maxdepth 1 \( -name 'cu.usb*' -o -name 'tty.usb*' -o -name 'ttyUSB*' -o -name 'ttyACM*' \) -print 2>/dev/null | sort)"; \
	if [ -n "$$ports" ]; then printf '%s\n' "$$ports"; else printf 'No likely ESP32 serial ports found.\n'; fi

board-info: ## Read the attached P4 silicon revision (requires PORT)
	@set -euo pipefail; \
	if [ -z "$(PORT)" ]; then printf 'PORT is required. Run make devices first.\n' >&2; exit 2; fi; \
	$(activate_idf); \
	revision="$$(./scripts/detect_p4_revision.sh "$(PORT)")"; \
	printf 'Selected firmware profile: %s\n' "$$revision"

configure: ## Configure the matching ESP32-P4 build
	$(call run_idf,set-target "$(TARGET)")

menuconfig: ## Open ESP-IDF configuration for the selected P4 profile
	$(call run_idf,menuconfig)

build: ## Build one P4 profile (auto uses PORT, otherwise pre3)
	$(call run_idf,build)

build-all: ## Build both mutually incompatible P4 silicon profiles
	@$(MAKE) build P4_REV=pre3
	@$(MAKE) build P4_REV=v3

firmware: build ## Alias for build

push: ## Build and flash the matching connected P4 (requires PORT)
	@if [ -z "$(PORT)" ]; then printf 'PORT is required. Run make devices first.\n' >&2; exit 2; fi
	$(call run_idf,$(FLASH_ARGS) build flash)

upload: push ## Alias for push
flash: push ## Alias for push

push-monitor: ## Build, flash, and monitor the matching P4 (requires PORT)
	@if [ -z "$(PORT)" ]; then printf 'PORT is required. Run make devices first.\n' >&2; exit 2; fi
	$(call run_idf,$(FLASH_ARGS) build flash monitor)

monitor: ## Open the serial monitor for the connected P4
	@if [ -z "$(PORT)" ]; then printf 'PORT is required. Run make devices first.\n' >&2; exit 2; fi
	$(call run_idf,$(PORT_ARG) monitor)

size: ## Show ESP-IDF firmware size details
	$(call run_idf,size)

size-check: build ## Verify the firmware fits the selected profile's app partition
	@set -euo pipefail; \
	$(activate_idf); \
	$(resolve_revision); \
	BUILD_DIR="$$build_dir" PARTITIONS_FILE="$$partitions" ./scripts/check_firmware_size.sh

test: ## Run host-side validation and tests
	@./scripts/run_tests.sh

check: test size-check ## Run host tests, firmware build, and partition-size check

sim-deps: ## Check dependencies required by the desktop simulator
	@command -v cmake >/dev/null 2>&1 || { printf 'cmake is required.\n' >&2; exit 1; }
	@command -v pkg-config >/dev/null 2>&1 || { printf 'pkg-config is required.\n' >&2; exit 1; }
	@pkg-config --exists sdl2 || { printf 'SDL2 development files are required (macOS: brew install sdl2 pkg-config).\n' >&2; exit 1; }
	@test -f managed_components/lvgl__lvgl/CMakeLists.txt || { printf 'LVGL is missing; run make build once to download managed components.\n' >&2; exit 1; }

sim-configure: sim-deps ## Configure the desktop simulator
	@cmake -S simulator -B "$(SIM_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

sim-build: sim-configure ## Build the desktop simulator
	@cmake --build "$(SIM_BUILD_DIR)" --target trimix_simulator

sim: sim-build ## Build and run the desktop simulator
	@"$(SIM_BUILD_DIR)/trimix_simulator" --zoom "$(ZOOM)"

emulator: sim ## Alias for sim
run: sim ## Alias for sim

sim-test: sim-configure ## Build and run the simulator test suite
	@cmake --build "$(SIM_BUILD_DIR)"
	@ctest --test-dir "$(SIM_BUILD_DIR)" --output-on-failure

clean: ## Remove both generated ESP32-P4 build directories
	@cmake -E remove_directory "$(FIRMWARE_BUILD_ROOT)/esp32p4-pre3"
	@cmake -E remove_directory "$(FIRMWARE_BUILD_ROOT)/esp32p4-v3"

clean-sim: ## Remove desktop simulator build outputs
	@cmake -E remove_directory "$(SIM_BUILD_DIR)"

clean-all: clean clean-sim ## Clean firmware and simulator build outputs
