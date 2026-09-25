#include "services/wifi_service.h"
#include "ui/components/status_icons.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr wifi_network_info_t kNetworks[] = {
    {"Trimix Lab", -38, 3, false},
    {"Workshop", -57, 3, false},
    {"Guest Net", -67, 0, false},
    {"Marina-2G", -76, 3, false},
    {"LongSSID-For-Layout-Testing-123", -83, 3, false},
};

bool g_initialized = false;
bool g_scanning = false;
bool g_connected = false;
char g_connected_ssid[33] = {};
char g_saved_ssid[33] = {};
char g_saved_password[65] = {};

wifi_signal_level_t signal_level_for_rssi(int8_t rssi) {
    return static_cast<wifi_signal_level_t>(wifi_service_rssi_to_bars(rssi));
}

int8_t current_rssi() {
    for (const auto& network : kNetworks) {
        if (std::strcmp(network.ssid, g_connected_ssid) == 0) {
            return network.rssi;
        }
    }
    return -55;
}

}  // namespace

void wifi_service_init(void) {
    g_initialized = true;
    status_set_wifi(g_connected, signal_level_for_rssi(current_rssi()));
}

void wifi_service_start_scan(void) {
    if (!g_initialized) return;
    g_scanning = false;
}

bool wifi_service_is_scanning(void) {
    return g_scanning;
}

bool wifi_service_is_ready(void) {
    return g_initialized;
}

uint16_t wifi_service_get_scan_count(void) {
    return static_cast<uint16_t>(std::size(kNetworks));
}

uint16_t wifi_service_get_scan_results(wifi_network_info_t* networks, uint16_t max_count) {
    if (!networks || max_count == 0) return 0;

    const uint16_t count = std::min<uint16_t>(max_count, static_cast<uint16_t>(std::size(kNetworks)));
    for (uint16_t i = 0; i < count; ++i) {
        networks[i] = kNetworks[i];
        networks[i].connected = g_connected && std::strcmp(networks[i].ssid, g_connected_ssid) == 0;
    }
    return count;
}

bool wifi_service_connect(const char* ssid, const char* password) {
    if (!g_initialized || !ssid || ssid[0] == '\0') return false;

    std::strncpy(g_connected_ssid, ssid, sizeof(g_connected_ssid) - 1);
    g_connected_ssid[sizeof(g_connected_ssid) - 1] = '\0';
    g_connected = true;
    wifi_service_save_credentials(ssid, password);
    status_set_wifi(true, signal_level_for_rssi(current_rssi()));
    return true;
}

void wifi_service_disconnect(void) {
    g_connected = false;
    g_connected_ssid[0] = '\0';
    status_set_wifi(false, WIFI_SIGNAL_NONE);
}

bool wifi_service_is_connected(void) {
    return g_connected;
}

bool wifi_service_get_connected_ssid(char* ssid) {
    if (!ssid || !g_connected) return false;
    std::strncpy(ssid, g_connected_ssid, 32);
    ssid[32] = '\0';
    return true;
}

int8_t wifi_service_get_rssi(void) {
    return g_connected ? current_rssi() : 0;
}

int wifi_service_rssi_to_bars(int8_t rssi) {
    if (rssi >= -50) return WIFI_SIGNAL_EXCELLENT;
    if (rssi >= -60) return WIFI_SIGNAL_GOOD;
    if (rssi >= -70) return WIFI_SIGNAL_FAIR;
    if (rssi >= -80) return WIFI_SIGNAL_WEAK;
    return WIFI_SIGNAL_NONE;
}

bool wifi_service_get_ip(char* ip_str) {
    if (!ip_str || !g_connected) return false;
    std::strncpy(ip_str, "192.168.10.42", 16);
    ip_str[15] = '\0';
    return true;
}

void wifi_service_save_credentials(const char* ssid, const char* password) {
    std::strncpy(g_saved_ssid, ssid ? ssid : "", sizeof(g_saved_ssid) - 1);
    g_saved_ssid[sizeof(g_saved_ssid) - 1] = '\0';
    std::strncpy(g_saved_password, password ? password : "", sizeof(g_saved_password) - 1);
    g_saved_password[sizeof(g_saved_password) - 1] = '\0';
}

bool wifi_service_load_credentials(char* ssid, char* password) {
    if (g_saved_ssid[0] == '\0') return false;
    if (ssid) {
        std::strncpy(ssid, g_saved_ssid, 32);
        ssid[32] = '\0';
    }
    if (password) {
        std::strncpy(password, g_saved_password, 64);
        password[64] = '\0';
    }
    return true;
}

void wifi_service_clear_credentials(void) {
    g_saved_ssid[0] = '\0';
    g_saved_password[0] = '\0';
}

void wifi_service_auto_connect(void) {
    char ssid[33] = {};
    char password[65] = {};
    if (wifi_service_load_credentials(ssid, password)) {
        wifi_service_connect(ssid, password);
    }
}
