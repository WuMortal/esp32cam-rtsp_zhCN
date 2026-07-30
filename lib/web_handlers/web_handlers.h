#pragma once

#include <Arduino.h>

class WebServer;
class Recorder;

void registerWebHandlers(WebServer &server, Recorder &recorder);

void handle_root();

#ifdef FLASH_LED_GPIO
void handle_flash();
#endif

void handle_snapshot();
void handle_stream();
void handle_restart();

void handle_api_config();
void handle_api_config_save();

void handle_wifi_status();
void handle_wifi_scan();
void handle_wifi_config_save();

void handle_storage_status();
void handle_snapshot_save();
void handle_video_control();
void handle_time_sync();

void handle_video_recording_loop();