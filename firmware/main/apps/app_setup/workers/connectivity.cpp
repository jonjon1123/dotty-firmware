/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <src/misc/lv_area.h>
#include <src/misc/lv_text.h>
#include <stackchan/stackchan.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <settings.h>
#include <hal/hal.h>
#include <memory>

using namespace smooth_ui_toolkit::lvgl_cpp;
using namespace setup_workers;
using namespace stackchan;

static std::string _tag = "Setup-Connectivity";

WifiSetupWorker::WifiSetupWorker()
{
    _state       = State::ShowInstructions;
    _last_state  = State::None;
    _is_first_in = true;

    // Create default avatar
    auto avatar = std::make_unique<avatar::DefaultAvatar>();
    avatar->init(lv_screen_active(), &lv_font_montserrat_24);
    avatar->leftEye().setVisible(false);
    avatar->rightEye().setVisible(false);
    avatar->mouth().setVisible(false);
    GetStackChan().attachAvatar(std::move(avatar));
}

WifiSetupWorker::~WifiSetupWorker()
{
    GetStackChan().resetAvatar();
}

void WifiSetupWorker::update()
{
    cleanup_ui();
    update_state();
}

void WifiSetupWorker::update_state()
{
    switch (_state) {
        case State::ShowInstructions: {
            if (_is_first_in) {
                _is_first_in = false;

                // Start the captive portal
                GetHAL().enterWifiConfigMode();

                auto& wifi = WifiManager::GetInstance();
                std::string ap_ssid = wifi.GetApSsid();
                std::string ap_url  = wifi.GetApWebUrl();

                auto& data = _state_instructions_data;

                data.panel = std::make_unique<Container>(lv_screen_active());
                data.panel->setBgColor(lv_color_hex(0xEDF4FF));
                data.panel->align(LV_ALIGN_CENTER, 0, 0);
                data.panel->setBorderWidth(0);
                data.panel->setSize(320, 240);
                data.panel->setRadius(0);
                data.panel->setPadding(0, 0, 0, 0);

                data.title = std::make_unique<Label>(lv_screen_active());
                data.title->setTextFont(&lv_font_montserrat_20);
                data.title->setTextColor(lv_color_hex(0x7E7B9C));
                data.title->align(LV_ALIGN_TOP_MID, 0, 5);
                data.title->setText("CONNECTIVITY SETUP");

                data.ap_name = std::make_unique<Label>(lv_screen_active());
                data.ap_name->setTextFont(&lv_font_montserrat_16);
                data.ap_name->setTextColor(lv_color_hex(0x26206A));
                data.ap_name->align(LV_ALIGN_TOP_MID, 0, 35);
                data.ap_name->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.ap_name->setText("Connect to Wi-Fi network:");

                data.info = std::make_unique<Label>(lv_screen_active());
                data.info->setTextFont(&lv_font_montserrat_24);
                data.info->setTextColor(lv_color_hex(0x26206A));
                data.info->align(LV_ALIGN_TOP_MID, 0, 55);
                data.info->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.info->setText(ap_ssid);

                data.url = std::make_unique<Label>(lv_screen_active());
                data.url->setTextFont(&lv_font_montserrat_16);
                data.url->setTextColor(lv_color_hex(0x525064));
                data.url->align(LV_ALIGN_TOP_MID, 0, 93);
                data.url->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.url->setText("Type into a browser:");

                data.url_value = std::make_unique<Label>(lv_screen_active());
                data.url_value->setTextFont(&lv_font_montserrat_24);
                data.url_value->setTextColor(lv_color_hex(0x26206A));
                data.url_value->align(LV_ALIGN_TOP_MID, 0, 115);
                data.url_value->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.url_value->setText(ap_url);

                data.hint = std::make_unique<Label>(lv_screen_active());
                data.hint->setTextFont(&lv_font_montserrat_16);
                data.hint->setTextColor(lv_color_hex(0x525064));
                data.hint->align(LV_ALIGN_TOP_MID, 0, 155);
                data.hint->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.hint->setText("Provide Wi-Fi Config and Custom\nOTA URL in the Advanced tab.");

                data.ota_example = std::make_unique<Label>(lv_screen_active());
                data.ota_example->setTextFont(&lv_font_montserrat_14);
                data.ota_example->setTextColor(lv_color_hex(0x525064));
                data.ota_example->setWidth(320);
                data.ota_example->alignTo(*data.hint, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
                data.ota_example->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.ota_example->setText("Example: http://<IP>:<PORT>/xiaozhi/ota/");
            }

            // The captive portal's OnExitRequested fires when user submits config
            // We detect this via the WifiManager exiting config mode
            {
                auto& wifi = WifiManager::GetInstance();
                if (!wifi.IsConfigMode() && !_config_exit_received) {
                    // Config mode was exited - user submitted credentials
                    _config_exit_received = true;
                    switch_state(State::VerifyConfig);
                }
            }

            break;
        }
        case State::WaitConfig: {
            // This state is no longer used - we go directly from ShowInstructions
            // to VerifyConfig when the captive portal exit is detected.
            break;
        }
        case State::VerifyConfig: {
            if (_is_first_in) {
                _is_first_in = false;

                auto& data = _state_verify_data;

                data.panel = std::make_unique<Container>(lv_screen_active());
                data.panel->setBgColor(lv_color_hex(0xEDF4FF));
                data.panel->align(LV_ALIGN_CENTER, 0, 0);
                data.panel->setBorderWidth(0);
                data.panel->setSize(320, 240);
                data.panel->setRadius(0);

                data.info = std::make_unique<Label>(lv_screen_active());
                data.info->setTextFont(&lv_font_montserrat_16);
                data.info->setTextColor(lv_color_hex(0x26206A));
                data.info->align(LV_ALIGN_CENTER, 0, 0);
                data.info->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.info->setText("Verifying connection...");

                // Run verification in a task to avoid blocking the UI
                if (verify_and_connect()) {
                    // Success - mark as configured and proceed
                    GetHAL().setAppConfiged(true);
                    switch_state(State::Done);
                } else {
                    switch_state(State::Failed);
                }
            }

            break;
        }
        case State::Done: {
            if (_is_first_in) {
                _is_first_in = false;

                auto& avatar = GetStackChan().avatar();
                avatar.leftEye().setVisible(true);
                avatar.rightEye().setVisible(true);
                avatar.mouth().setVisible(true);
                avatar.setEmotion(avatar::Emotion::Happy);

                _state_done_data.reboot_count = 4;
            }

            if (GetHAL().millis() - _last_tick > 1000) {
                _last_tick = GetHAL().millis();
                if (_state_done_data.reboot_count > 0) {
                    _state_done_data.reboot_count--;
                    auto& avatar = GetStackChan().avatar();
                    avatar.setSpeech(fmt::format("Done!  Reboot in {}s.", _state_done_data.reboot_count));
                } else {
                    mclog::tagInfo(_tag, "rebooting...");
                    GetHAL().delay(100);
                    GetHAL().reboot();
                }
            }

            break;
        }
        case State::Failed: {
            if (_is_first_in) {
                _is_first_in = false;

                auto& data = _state_failed_data;

                data.panel = std::make_unique<Container>(lv_screen_active());
                data.panel->setBgColor(lv_color_hex(0xEDF4FF));
                data.panel->align(LV_ALIGN_CENTER, 0, 0);
                data.panel->setBorderWidth(0);
                data.panel->setSize(320, 240);
                data.panel->setRadius(0);

                data.info = std::make_unique<Label>(lv_screen_active());
                data.info->setTextFont(&lv_font_montserrat_16);
                data.info->setTextColor(lv_color_hex(0xCC0000));
                data.info->align(LV_ALIGN_TOP_MID, 0, 30);
                data.info->setTextAlign(LV_TEXT_ALIGN_CENTER);
                data.info->setText("Setup failed!\nWi-Fi or OTA URL\nis not configured.");

                data.btn_retry = std::make_unique<Button>(lv_screen_active());
                apply_button_common_style(*data.btn_retry);
                data.btn_retry->align(LV_ALIGN_CENTER, 0, 60);
                data.btn_retry->setSize(160, 48);
                data.btn_retry->label().setText("Try Again");
                data.btn_retry->label().setTextFont(&lv_font_montserrat_20);
                data.btn_retry->onClick().connect([this]() { _state_failed_data.retry_clicked = true; });
            }

            if (_state_failed_data.retry_clicked) {
                _config_exit_received = false;
                switch_state(State::ShowInstructions);
            }

            break;
        }
        default:
            break;
    }
}

bool WifiSetupWorker::verify_and_connect()
{
    mclog::tagInfo(_tag, "starting verification: WiFi + OTA URL");

    // Step 1: Connect to WiFi
    auto& avatar = GetStackChan().avatar();
    avatar.setSpeech("Connecting to Wi-Fi...");

    if (!GetHAL().waitForWifiConnected(30000)) {
        mclog::tagError(_tag, "WiFi connection failed during verification");
        return false;
    }

    // Step 2: Read OTA URL from NVS and verify it
    avatar.setSpeech("Checking OTA server...");

    Settings settings("wifi", false);
    std::string ota_url = settings.GetString("ota_url");

    if (ota_url.empty()) {
        mclog::tagError(_tag, "OTA URL is not set in NVS");
        return false;
    }

    if (!GetHAL().verifyOtaUrl(ota_url)) {
        mclog::tagError(_tag, "OTA URL verification failed: {}", ota_url);
        return false;
    }

    mclog::tagInfo(_tag, "all checks passed: WiFi connected, OTA URL verified");
    avatar.setSpeech("Setup complete!");
    return true;
}

void WifiSetupWorker::cleanup_ui()
{
    if (_last_state == State::None) {
        return;
    }

    switch (_last_state) {
        case State::ShowInstructions: {
            _state_instructions_data.reset();
            break;
        }
        case State::WaitConfig: {
            break;
        }
        case State::VerifyConfig: {
            _state_verify_data.reset();
            break;
        }
        case State::Done: {
            break;
        }
        case State::Failed: {
            _state_failed_data.reset();
            break;
        }
        default:
            break;
    }

    _last_state = State::None;
}

void WifiSetupWorker::switch_state(State newState)
{
    _last_state  = _state;
    _state       = newState;
    _is_first_in = true;
}
