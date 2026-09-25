#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi network info structure
typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;  // 0=open, 1=WEP, 2=WPA, 3=WPA2, etc.
    bool connected;
} wifi_network_info_t;

/**
 * Initialize WiFi service
 * Must be called before using any other WiFi functions
 */
void wifi_service_init(void);

/**
 * Start WiFi scan for available networks
 * Results available via wifi_service_get_scan_results after scan completes
 */
void wifi_service_start_scan(void);

/**
 * Check if scan is in progress
 */
bool wifi_service_is_scanning(void);

/**
 * Check if WiFi service is ready
 */
bool wifi_service_is_ready(void);

/**
 * Get number of networks found in last scan
 */
uint16_t wifi_service_get_scan_count(void);

/**
 * Get scan results
 * @param networks Array to fill with network info
 * @param max_count Maximum number of entries to return
 * @return Actual number of networks returned
 */
uint16_t wifi_service_get_scan_results(wifi_network_info_t* networks, uint16_t max_count);

/**
 * Connect to a WiFi network
 * @param ssid Network SSID
 * @param password Network password (NULL for open networks)
 * @return true if connection attempt started
 */
bool wifi_service_connect(const char* ssid, const char* password);

/**
 * Disconnect from current network
 */
void wifi_service_disconnect(void);

/**
 * Check if connected to WiFi
 */
bool wifi_service_is_connected(void);

/**
 * Get current connection SSID
 * @param ssid Buffer to fill (at least 33 bytes)
 * @return true if connected and SSID retrieved
 */
bool wifi_service_get_connected_ssid(char* ssid);

/**
 * Get current signal strength
 * @return RSSI value in dBm, or 0 if not connected
 */
int8_t wifi_service_get_rssi(void);

/**
 * Convert RSSI to signal level for UI
 */
int wifi_service_rssi_to_bars(int8_t rssi);

/**
 * Get IP address as string
 * @param ip_str Buffer to fill (at least 16 bytes)
 * @return true if connected and IP retrieved
 */
bool wifi_service_get_ip(char* ip_str);

/**
 * Save credentials for auto-connect
 */
void wifi_service_save_credentials(const char* ssid, const char* password);

/**
 * Load saved credentials
 * @return true if credentials exist
 */
bool wifi_service_load_credentials(char* ssid, char* password);

/**
 * Clear saved credentials
 */
void wifi_service_clear_credentials(void);

/**
 * Attempt to auto-connect using saved credentials
 */
void wifi_service_auto_connect(void);

#ifdef __cplusplus
}
#endif
