#pragma once
#include <cstdint>

// Electrical contract for the reviewed main PCB. Physical JP1 connector pitch,
// pin-1/mating view and harness current rating remain measurement gates.
// verify_system_contract.py reconciles these definitions against the netlist.
namespace hardware_contract {
struct HostPin { uint8_t gpio; uint8_t j301; };
constexpr HostPin kSda{28,21};
constexpr HostPin kScl{29,14};
constexpr HostPin kCoTx{30,12};
constexpr HostPin kCoRx{31,10};
constexpr HostPin kPowerInterrupt{32,19};
constexpr HostPin kPowerKill{33,8};
constexpr HostPin kHeliumEnable{34,17};
constexpr HostPin kChargerAlert{49,13}; // Shared active-low charger IRQ / USB latch feedback.
constexpr HostPin kUsbPermission{50,11}; // Fail-low USB input-current authorization.
constexpr HostPin kCoEnable{51,9};
constexpr HostPin kUnusedOxygenSelector{52,7}; // Never configured or driven.
constexpr uint8_t kGaugeAddress=0x36;
constexpr uint8_t kOxygenAddress=0x40;
constexpr uint8_t kHeliumAddress=0x41;
constexpr uint8_t kChargerAddress=0x6a;
constexpr uint8_t kUsbCcAddress=0x47;
constexpr uint8_t kUsbBc12Address=0x5f;
constexpr uint8_t kEnvironmentAddress0=0x76;
constexpr uint8_t kEnvironmentAddress1=0x77;
constexpr uint32_t kI2cFrequencyHz=100000;
constexpr uint32_t kCoBaud=9600;
}
