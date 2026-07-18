/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <stackchan/stackchan.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <board.h>
#include <settings.h>
#include <mutex>
#include <queue>
#include <vector>
#include <ctime>
#include <sys/time.h>
#include <esp_sntp.h>
#include <esp_http_client.h>
#include <atomic>

static std::string _tag           = "Network";
static bool _is_network_connected = false;
// Set true when esp-sntp confirms a sync via the notification cb.
// Read via Hal::isTimeSynced() so the status bar can hide the clock until
// the on-board RTC has been corrected (PCF8563 boots stale when the coin
// battery is missing/depleted).
static std::atomic<bool> _sntp_synced{false};

static void time_sync_notification_cb(struct timeval* tv)
{
    mclog::tagInfo(_tag, "SNTP time synchronized");
    GetHAL().syncSystemTimeToRtc();
    _sntp_synced.store(true, std::memory_order_release);
}

bool Hal::isTimeSynced() const
{
    return _sntp_synced.load(std::memory_order_acquire);
}

void Hal::startSntp()
{
    mclog::tagInfo(_tag, "SNTP init");

    if (esp_sntp_enabled()) {
    } else {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
        esp_sntp_setservername(2, "cn.pool.ntp.org");

        sntp_set_time_sync_notification_cb(time_sync_notification_cb);

        esp_sntp_init();
    }
}

void Hal::startNetwork(std::function<void(std::string_view)> onLog)
{
    if (_is_network_connected) {
        mclog::tagInfo(_tag, "network already connected");
        return;
    }

    std::atomic<bool> network_connected = false;

    auto& board = Board::GetInstance();
    mclog::tagInfo(_tag, "start and wait for network connected...");

    board.SetNetworkEventCallback([&network_connected, &onLog](NetworkEvent event, const std::string& data) {
        switch (event) {
            case NetworkEvent::Scanning:
                if (onLog) {
                    onLog("WiFi scanning...");
                }
                break;
            case NetworkEvent::Connecting: {
                if (data.empty()) {
                    if (onLog) {
                        onLog("WiFi connecting...");
                    }
                } else {
                    if (onLog) {
                        onLog(fmt::format("Connecting to {} ...", data));
                    }
                }
                break;
            }
            case NetworkEvent::Connected: {
                network_connected = true;
                break;
            }
            case NetworkEvent::Disconnected:
                break;
            case NetworkEvent::WifiConfigModeEnter: {
                auto& wifi_manager = WifiManager::GetInstance();
                auto msg = fmt::format("Enter WiFi config mode. Hotspot: {}, Config URL: {}", wifi_manager.GetApSsid(),
                                       wifi_manager.GetApWebUrl());
                if (onLog) {
                    onLog(msg);
                }
                break;
            }
            case NetworkEvent::WifiConfigModeExit:
                // WiFi config mode exit is handled by WifiBoard internally
                break;
            // Cellular modem specific events
            case NetworkEvent::ModemDetecting:
                break;
            case NetworkEvent::ModemErrorNoSim:
                break;
            case NetworkEvent::ModemErrorRegDenied:
                break;
            case NetworkEvent::ModemErrorInitFailed:
                break;
            case NetworkEvent::ModemErrorTimeout:
                break;
        }
    });
    board.StartNetwork();

    while (!network_connected) {
        GetHAL().delay(500);
    }
    mclog::tagInfo(_tag, "network connected");
    board.SetNetworkEventCallback(nullptr);

    startSntp();

    _is_network_connected = true;
}

WifiStatus Hal::getWifiStatus()
{
    auto& wifi = WifiManager::GetInstance();

    if (wifi.IsConfigMode()) {
        return WifiStatus::None;
    }
    if (!wifi.IsConnected()) {
        return WifiStatus::None;
    }

    int rssi = wifi.GetRssi();
    if (rssi >= -65) {
        return WifiStatus::High;
    } else if (rssi >= -75) {
        return WifiStatus::Medium;
    }
    return WifiStatus::Low;
}

void Hal::enterWifiConfigMode()
{
    mclog::tagInfo(_tag, "entering WiFi config mode (captive portal)");
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized()) {
        mclog::tagInfo(_tag, "initializing WifiManager");
        WifiManagerConfig config;
        config.ssid_prefix = "StackChan";
        config.language = "en-US";
        wifi.Initialize(config);
    }
    wifi.StartConfigAp();
}

bool Hal::waitForWifiConnected(uint32_t timeout_ms)
{
    mclog::tagInfo(_tag, "waiting for WiFi connection, timeout={}ms", timeout_ms);
    auto& wifi = WifiManager::GetInstance();

    // If already connected, return immediately
    if (wifi.IsConnected()) {
        mclog::tagInfo(_tag, "WiFi already connected");
        return true;
    }

    // Start station mode to try connecting with saved credentials
    wifi.StartStation();

    uint32_t start = millis();
    while (!wifi.IsConnected()) {
        delay(500);
        if (millis() - start > timeout_ms) {
            mclog::tagInfo(_tag, "WiFi connection timed out");
            return false;
        }
    }

    mclog::tagInfo(_tag, "WiFi connected successfully");
    return true;
}

bool Hal::verifyOtaUrl(const std::string& url)
{
    if (url.empty()) {
        mclog::tagInfo(_tag, "OTA URL is empty");
        return false;
    }

    mclog::tagInfo(_tag, "verifying OTA URL: {}", url);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = 5000;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        mclog::tagError(_tag, "failed to create HTTP client for OTA URL verification");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        mclog::tagError(_tag, "failed to connect to OTA URL: {}", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Accept any HTTP response (2xx, 3xx, even 4xx means server is reachable)
    if (status_code >= 200 && status_code < 500) {
        mclog::tagInfo(_tag, "OTA URL verified, status={}", status_code);
        return true;
    }

    mclog::tagError(_tag, "OTA URL returned error status={}", status_code);
    return false;
}

void Hal::setAppConfiged(bool configured)
{
    Settings settings("app_config", true);
    settings.SetBool("is_configed", configured);
    mclog::tagInfo(_tag, "app_configed set to {}", configured);
}
