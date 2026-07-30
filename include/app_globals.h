#pragma once

#include <Arduino.h>
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <OV2640.h>
#include <WebServer.h>
#include <storage.h>
#include <recorder.h>
#include <rtsp_server.h>
#include <lookup_camera_effect.h>
#include <lookup_camera_frame_size.h>
#include <lookup_camera_gainceiling.h>
#include <lookup_camera_wb_mode.h>
#include <memory>
#include "settings.h"

extern const char index_html_min_start[] asm("_binary_html_index_min_html_start");

extern OV2640 cam;
extern DNSServer dnsServer;
extern WebServer web_server;
extern std::unique_ptr<rtsp_server> camera_server;
extern IotWebConf iotWebConf;
extern esp_err_t camera_init_result;
extern volatile bool recordingDesired;
extern Recorder *recorder;

extern iotwebconf::ParameterGroup param_group_camera;
extern iotwebconf::ParameterGroup param_group_storage;

extern iotwebconf::UIntTParameter<unsigned long> param_frame_duration;
extern iotwebconf::SelectTParameter<sizeof(frame_sizes[0])> param_frame_size;
extern iotwebconf::UIntTParameter<byte> param_jpg_quality;
extern iotwebconf::IntTParameter<int> param_brightness;
extern iotwebconf::IntTParameter<int> param_contrast;
extern iotwebconf::IntTParameter<int> param_saturation;
extern iotwebconf::SelectTParameter<sizeof(camera_effects[0])> param_special_effect;
extern iotwebconf::CheckboxTParameter param_whitebal;
extern iotwebconf::CheckboxTParameter param_awb_gain;
extern iotwebconf::SelectTParameter<sizeof(camera_wb_modes[0])> param_wb_mode;
extern iotwebconf::CheckboxTParameter param_exposure_ctrl;
extern iotwebconf::CheckboxTParameter param_aec2;
extern iotwebconf::IntTParameter<int> param_ae_level;
extern iotwebconf::IntTParameter<int> param_aec_value;
extern iotwebconf::CheckboxTParameter param_gain_ctrl;
extern iotwebconf::IntTParameter<int> param_agc_gain;
extern iotwebconf::SelectTParameter<sizeof(camera_gain_ceilings[0])> param_gain_ceiling;
extern iotwebconf::CheckboxTParameter param_bpc;
extern iotwebconf::CheckboxTParameter param_wpc;
extern iotwebconf::CheckboxTParameter param_raw_gma;
extern iotwebconf::CheckboxTParameter param_lenc;
extern iotwebconf::CheckboxTParameter param_hmirror;
extern iotwebconf::CheckboxTParameter param_vflip;
extern iotwebconf::CheckboxTParameter param_dcw;
extern iotwebconf::CheckboxTParameter param_colorbar;

extern iotwebconf::CheckboxTParameter param_storage_enabled;
extern iotwebconf::TextTParameter<sizeof(DEFAULT_STORAGE_PATH)> param_storage_path;
extern iotwebconf::CheckboxTParameter param_video_enabled;
extern iotwebconf::UIntTParameter<unsigned long> param_video_duration;
extern iotwebconf::UIntTParameter<unsigned long> param_video_max_size;

void update_camera_settings();
