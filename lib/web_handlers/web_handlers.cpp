#include "web_handlers.h"
#include "app_globals.h"
#include <WiFi.h>
#include <rtsp_server.h>
#include <settings.h>
#include <format_duration.h>
#include <format_number.h>
#include <moustache.h>
#include <ESPmDNS.h>
#include <time.h>
#include <sys/time.h>

static constexpr size_t FRAME_SIZE_NAME_LEN = sizeof(frame_sizes[0].name);
static constexpr size_t EFFECT_NAME_LEN = sizeof(camera_effects[0].name);
static constexpr size_t WB_MODE_NAME_LEN = sizeof(camera_wb_modes[0].name);
static constexpr size_t GAIN_CEILING_NAME_LEN = sizeof(camera_gain_ceilings[0].name);
static constexpr size_t STORAGE_PATH_LEN = sizeof(DEFAULT_STORAGE_PATH);

static WebServer *srv = nullptr;
static Recorder *rec = nullptr;

void registerWebHandlers(WebServer &server, Recorder &recorder)
{
  srv = &server;
  rec = &recorder;

  srv->on("/", HTTP_GET, handle_root);
  srv->on("/config", []()
          { iotWebConf.handleConfig(); });
  srv->on("/snapshot", HTTP_GET, handle_snapshot);
  srv->on("/stream", HTTP_GET, handle_stream);
  srv->on("/api/config", HTTP_GET, handle_api_config);
  srv->on("/api/config", HTTP_POST, handle_api_config_save);
  srv->on("/api/wifi/status", HTTP_GET, handle_wifi_status);
  srv->on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
  srv->on("/api/wifi/config", HTTP_POST, handle_wifi_config_save);
  srv->on("/api/storage/status", HTTP_GET, handle_storage_status);
  srv->on("/api/storage/snapshot", HTTP_POST, handle_snapshot_save);
  srv->on("/api/storage/video", HTTP_POST, handle_video_control);
  srv->on("/api/time/sync", HTTP_POST, handle_time_sync);
#ifdef FLASH_LED_GPIO
  srv->on("/flash", HTTP_GET, handle_flash);
#endif
  srv->on("/restart", HTTP_GET, handle_restart);
  srv->onNotFound([]()
                  { iotWebConf.handleNotFound(); });
}

// ==================== helpers ====================

template <typename T, size_t N>
static int findOptionIndex(const T (&options)[N], const char *value)
{
  for (size_t i = 0; i < N; i++)
  {
    if (strncmp(options[i].name, value, sizeof(options[i].name)) == 0)
      return (int)i;
  }
  return 0;
}

// ==================== root ====================

void handle_root()
{
  log_v("Handle root");
  if (iotWebConf.handleCaptivePortal())
    return;

  auto hostname = "esp32-" + WiFi.macAddress() + ".local";
  hostname.replace(":", "");
  hostname.toLowerCase();

  const char *wifi_modes[] = {"NULL", "STA", "AP", "STA+AP"};
  auto ipv4 = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIP() : WiFi.localIP();
  auto ipv6 = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIPv6() : WiFi.localIPv6();

  const char *initResult = esp_err_to_name(camera_init_result);
  if (initResult == nullptr)
    initResult = "Unknown reason";

  moustache_variable_t substitutions[] = {
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"BoardType", BOARD_NAME},
      {"ThingName", iotWebConf.getThingName()},
      {"SDKVersion", ESP.getSdkVersion()},
      {"ChipModel", ESP.getChipModel()},
      {"ChipRevision", String(ESP.getChipRevision())},
      {"CpuFreqMHz", String(ESP.getCpuFreqMHz())},
      {"CpuCores", String(ESP.getChipCores())},
      {"FlashSize", format_memory(ESP.getFlashChipSize(), 0)},
      {"HeapSize", format_memory(ESP.getHeapSize())},
      {"PsRamSize", format_memory(ESP.getPsramSize(), 0)},
      {"Uptime", String(format_duration(millis() / 1000))},
      {"FreeHeap", format_memory(ESP.getFreeHeap())},
      {"MaxAllocHeap", format_memory(ESP.getMaxAllocHeap())},
      {"NumRTSPSessions", camera_server != nullptr ? String(camera_server->num_connected()) : "RTSP server disabled"},
      {"HostName", hostname},
      {"MacAddress", WiFi.macAddress()},
      {"AccessPoint", WiFi.SSID()},
      {"SignalStrength", String(WiFi.RSSI())},
      {"WifiMode", wifi_modes[WiFi.getMode()]},
      {"IPv4", ipv4.toString()},
      {"IPv6", ipv6.toString()},
      {"NetworkState.ApMode", String(iotWebConf.getState() == iotwebconf::NetworkState::ApMode)},
      {"NetworkState.OnLine", String(iotWebConf.getState() == iotwebconf::NetworkState::OnLine)},
      {"FrameSize", String(param_frame_size.value())},
      {"FrameDuration", String(param_frame_duration.value())},
      {"FrameFrequency", String(1000.0 / param_frame_duration.value(), 1)},
      {"JpegQuality", String(param_jpg_quality.value())},
      {"CameraInitialized", String(camera_init_result == ESP_OK)},
      {"CameraInitResult", String(camera_init_result)},
      {"CameraInitResultText", initResult},
      {"Brightness", String(param_brightness.value())},
      {"Contrast", String(param_contrast.value())},
      {"Saturation", String(param_saturation.value())},
      {"SpecialEffect", String(param_special_effect.value())},
      {"WhiteBal", String(param_whitebal.value())},
      {"AwbGain", String(param_awb_gain.value())},
      {"WbMode", String(param_wb_mode.value())},
      {"ExposureCtrl", String(param_exposure_ctrl.value())},
      {"Aec2", String(param_aec2.value())},
      {"AeLevel", String(param_ae_level.value())},
      {"AecValue", String(param_aec_value.value())},
      {"GainCtrl", String(param_gain_ctrl.value())},
      {"AgcGain", String(param_agc_gain.value())},
      {"GainCeiling", String(param_gain_ceiling.value())},
      {"Bpc", String(param_bpc.value())},
      {"Wpc", String(param_wpc.value())},
      {"RawGma", String(param_raw_gma.value())},
      {"Lenc", String(param_lenc.value())},
      {"HMirror", String(param_hmirror.value())},
      {"VFlip", String(param_vflip.value())},
      {"Dcw", String(param_dcw.value())},
      {"ColorBar", String(param_colorbar.value())},
      {"RtspPort", String(RTSP_PORT)},
      {"StorageEnabled", String(param_storage_enabled.value() ? "true" : "false")},
      {"StoragePath", String(param_storage_path.value())},
      {"VideoEnabled", String(param_video_enabled.value() ? "true" : "false")},
      {"VideoDuration", String(param_video_duration.value())},
      {"VideoMaxSize", String(param_video_max_size.value())},
      {"StorageTotal", format_memory(storage.getTotalBytes())},
      {"StorageUsed", format_memory(storage.getUsedBytes())},
      {"StorageFree", format_memory(storage.getFreeBytes())},
      {"StorageInitialized", String(storage.isInitialized() ? "true" : "false")},
      {"StorageMounted", String(storage.isMounted() ? "true" : "false")},
      {"SnapshotCount", String(storage.getSnapshotCount())},
      {"VideoRecording", String(recordingDesired ? "true" : "false")}};

  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  auto html = moustache_render(index_html_min_start, substitutions);
  srv->send(200, "text/html", html);
}

#ifdef FLASH_LED_GPIO
void handle_flash()
{
  log_v("handle_flash");
  auto v = srv->hasArg("v") ? srv->arg("v").toInt() : 0;
  analogWrite(FLASH_LED_GPIO, v);
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->send(200);
}
#endif

void handle_snapshot()
{
  log_v("handle_snapshot");
  if (camera_init_result != ESP_OK)
  {
    srv->send(404, "text/plain", "Camera is not initialized");
    return;
  }
  auto frame_buffers = CAMERA_CONFIG_FB_COUNT;
  while (frame_buffers--)
    cam.run();
  auto fb_len = cam.getSize();
  auto fb = (const char *)cam.getfb();
  if (fb == nullptr)
  {
    srv->send(404, "text/plain", "Unable to obtain frame buffer from the camera");
    return;
  }
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->setContentLength(fb_len);
  srv->send(200, "image/jpeg", "");
  srv->sendContent(fb, fb_len);
}

#define STREAM_CONTENT_BOUNDARY "123456789000000000000987654321"

void handle_stream()
{
  log_v("handle_stream");
  if (camera_init_result != ESP_OK)
  {
    srv->send(404, "text/plain", "Camera is not initialized");
    return;
  }
  if (recordingDesired)
  {
    srv->send(503, "text/plain", "Streaming unavailable while recording");
    return;
  }
  log_v("starting streaming");
  char size_buf[12];
  auto client = srv->client();
  client.write("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: multipart/x-mixed-replace; boundary=" STREAM_CONTENT_BOUNDARY "\r\n");
  while (client.connected())
  {
    yield();
    client.write("\r\n--" STREAM_CONTENT_BOUNDARY "\r\n");
    cam.run();
    client.write("Content-Type: image/jpeg\r\nContent-Length: ");
    sprintf(size_buf, "%d\r\n\r\n", cam.getSize());
    client.write(size_buf);
    client.write(cam.getfb(), cam.getSize());
  }
  log_v("client disconnected");
  client.stop();
  log_v("stopped streaming");
}

void handle_restart()
{
  log_v("handle_restart");
  WiFi.disconnect(false, true);
  ESP.restart();
}

// ==================== /api/config (GET) ====================

void handle_api_config()
{
  log_v("handle_api_config");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"frame_duration\":" + String(param_frame_duration.value()) + ",";
  json += "\"frame_size\":\"" + String(param_frame_size.value()) + "\",";
  json += "\"frame_size_index\":" + String(findOptionIndex(frame_sizes, param_frame_size.value())) + ",";
  json += "\"jpg_quality\":" + String(param_jpg_quality.value()) + ",";
  json += "\"brightness\":" + String(param_brightness.value()) + ",";
  json += "\"contrast\":" + String(param_contrast.value()) + ",";
  json += "\"saturation\":" + String(param_saturation.value()) + ",";
  json += "\"special_effect\":\"" + String(param_special_effect.value()) + "\",";
  json += "\"special_effect_index\":" + String(findOptionIndex(camera_effects, param_special_effect.value())) + ",";
  json += "\"whitebal\":" + String(param_whitebal.value() ? "true" : "false") + ",";
  json += "\"awb_gain\":" + String(param_awb_gain.value() ? "true" : "false") + ",";
  json += "\"wb_mode\":\"" + String(param_wb_mode.value()) + "\",";
  json += "\"wb_mode_index\":" + String(findOptionIndex(camera_wb_modes, param_wb_mode.value())) + ",";
  json += "\"exposure_ctrl\":" + String(param_exposure_ctrl.value() ? "true" : "false") + ",";
  json += "\"aec2\":" + String(param_aec2.value() ? "true" : "false") + ",";
  json += "\"ae_level\":" + String(param_ae_level.value()) + ",";
  json += "\"aec_value\":" + String(param_aec_value.value()) + ",";
  json += "\"gain_ctrl\":" + String(param_gain_ctrl.value() ? "true" : "false") + ",";
  json += "\"agc_gain\":" + String(param_agc_gain.value()) + ",";
  json += "\"gain_ceiling\":\"" + String(param_gain_ceiling.value()) + "\",";
  json += "\"gain_ceiling_index\":" + String(findOptionIndex(camera_gain_ceilings, param_gain_ceiling.value())) + ",";
  json += "\"bpc\":" + String(param_bpc.value() ? "true" : "false") + ",";
  json += "\"wpc\":" + String(param_wpc.value() ? "true" : "false") + ",";
  json += "\"raw_gma\":" + String(param_raw_gma.value() ? "true" : "false") + ",";
  json += "\"lenc\":" + String(param_lenc.value() ? "true" : "false") + ",";
  json += "\"hmirror\":" + String(param_hmirror.value() ? "true" : "false") + ",";
  json += "\"vflip\":" + String(param_vflip.value() ? "true" : "false") + ",";
  json += "\"dcw\":" + String(param_dcw.value() ? "true" : "false") + ",";
  json += "\"colorbar\":" + String(param_colorbar.value() ? "true" : "false") + ",";
  json += "\"storage_enabled\":" + String(param_storage_enabled.value() ? "true" : "false") + ",";
  json += "\"storage_path\":\"" + String(param_storage_path.value()) + "\",";
  json += "\"video_enabled\":" + String(param_video_enabled.value() ? "true" : "false") + ",";
  json += "\"video_duration\":" + String(param_video_duration.value()) + ",";
  json += "\"video_max_size\":" + String(param_video_max_size.value());
  json += "}";

  srv->send(200, "application/json", json);
}

// ==================== /api/config (POST) ====================

void handle_api_config_save()
{
  log_v("handle_api_config_save");
  const bool cameraReady = (camera_init_result == ESP_OK);

  if (srv->hasArg("fd"))
    param_frame_duration.value() = srv->arg("fd").toInt();
  if (srv->hasArg("fs"))
  {
    strncpy(param_frame_size.value(), srv->arg("fs").c_str(), FRAME_SIZE_NAME_LEN - 1);
    param_frame_size.value()[FRAME_SIZE_NAME_LEN - 1] = '\0';
  }
  if (srv->hasArg("q"))
    param_jpg_quality.value() = (byte)srv->arg("q").toInt();
  if (srv->hasArg("b"))
    param_brightness.value() = (int8_t)srv->arg("b").toInt();
  if (srv->hasArg("c"))
    param_contrast.value() = (int8_t)srv->arg("c").toInt();
  if (srv->hasArg("s"))
    param_saturation.value() = (int8_t)srv->arg("s").toInt();
  if (srv->hasArg("e"))
  {
    strncpy(param_special_effect.value(), srv->arg("e").c_str(), EFFECT_NAME_LEN - 1);
    param_special_effect.value()[EFFECT_NAME_LEN - 1] = '\0';
  }
  if (srv->hasArg("wb"))
    param_whitebal.value() = (srv->arg("wb") == "1");
  if (srv->hasArg("awbg"))
    param_awb_gain.value() = (srv->arg("awbg") == "1");
  if (srv->hasArg("wbm"))
  {
    strncpy(param_wb_mode.value(), srv->arg("wbm").c_str(), WB_MODE_NAME_LEN - 1);
    param_wb_mode.value()[WB_MODE_NAME_LEN - 1] = '\0';
  }
  if (srv->hasArg("ec"))
    param_exposure_ctrl.value() = (srv->arg("ec") == "1");
  if (srv->hasArg("aec2"))
    param_aec2.value() = (srv->arg("aec2") == "1");
  if (srv->hasArg("ael"))
    param_ae_level.value() = (int8_t)srv->arg("ael").toInt();
  if (srv->hasArg("aecv"))
    param_aec_value.value() = (uint16_t)srv->arg("aecv").toInt();
  if (srv->hasArg("gc"))
    param_gain_ctrl.value() = (srv->arg("gc") == "1");
  if (srv->hasArg("agcg"))
    param_agc_gain.value() = (uint8_t)srv->arg("agcg").toInt();
  if (srv->hasArg("gcl"))
  {
    strncpy(param_gain_ceiling.value(), srv->arg("gcl").c_str(), GAIN_CEILING_NAME_LEN - 1);
    param_gain_ceiling.value()[GAIN_CEILING_NAME_LEN - 1] = '\0';
  }
  if (srv->hasArg("bpc"))
    param_bpc.value() = (srv->arg("bpc") == "1");
  if (srv->hasArg("wpc"))
    param_wpc.value() = (srv->arg("wpc") == "1");
  if (srv->hasArg("rg"))
    param_raw_gma.value() = (srv->arg("rg") == "1");
  if (srv->hasArg("lenc"))
    param_lenc.value() = (srv->arg("lenc") == "1");
  if (srv->hasArg("hm"))
    param_hmirror.value() = (srv->arg("hm") == "1");
  if (srv->hasArg("vm"))
    param_vflip.value() = (srv->arg("vm") == "1");
  if (srv->hasArg("dcw"))
    param_dcw.value() = (srv->arg("dcw") == "1");
  if (srv->hasArg("cb"))
    param_colorbar.value() = (srv->arg("cb") == "1");

  // 存储设置
  if (srv->hasArg("ste"))
    param_storage_enabled.value() = (srv->arg("ste") == "1");
  if (srv->hasArg("stp"))
  {
    strncpy(param_storage_path.value(), srv->arg("stp").c_str(), sizeof(param_storage_path.value()) - 1);
    param_storage_path.value()[sizeof(param_storage_path.value()) - 1] = '\0';
  }
  if (srv->hasArg("ve"))
    param_video_enabled.value() = (srv->arg("ve") == "1");
  if (srv->hasArg("vd"))
    param_video_duration.value() = (unsigned long)srv->arg("vd").toInt();
  if (srv->hasArg("vms"))
    param_video_max_size.value() = (unsigned long)srv->arg("vms").toInt();

  storage.setEnabled(param_storage_enabled.value());
  storage.setBasePath(param_storage_path.value());
  storage.setVideoMaxDuration(param_video_duration.value());
  storage.setVideoMaxFileSize(param_video_max_size.value() * 1024);

  bool sdOk = true;
  if (param_storage_enabled.value())
    sdOk = storage.begin();
  else
    storage.end();

  iotWebConf.saveConfig();
  if (cameraReady)
    update_camera_settings();

  if (param_storage_enabled.value() && !sdOk)
    srv->send(200, "application/json", "{\"success\":true,\"warning\":\"SD卡初始化失败，请检查SD卡是否插入\"}");
  else
    srv->send(200, "application/json", "{\"success\":true}");
}

// ==================== WiFi APIs ====================

void handle_wifi_status()
{
  log_v("handle_wifi_status");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  String json = "{";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  srv->send(200, "application/json", json);
}

void handle_wifi_scan()
{
  log_v("handle_wifi_scan");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  int n = WiFi.scanNetworks(false, false);
  String json = "[";
  for (int i = 0; i < n; i++)
  {
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  json += "]";
  srv->send(200, "application/json", json);
  WiFi.scanDelete();
}

void handle_wifi_config_save()
{
  log_v("handle_wifi_config_save");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  String ssid = "";
  String password = "";
  if (srv->hasArg("ssid"))
    ssid = srv->arg("ssid");
  if (srv->hasArg("password"))
    password = srv->arg("password");
  if (ssid.length() == 0)
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"SSID is required\"}");
    return;
  }
  auto *wifiParams = iotWebConf.getWifiParameterGroup();
  if (wifiParams->_wifiSsid[0] == '\0')
  {
    srv->send(500, "application/json", "{\"success\":false,\"error\":\"WiFi settings not available\"}");
    return;
  }
  strncpy(wifiParams->_wifiSsid, ssid.c_str(), sizeof(wifiParams->_wifiSsid) - 1);
  wifiParams->_wifiSsid[sizeof(wifiParams->_wifiSsid) - 1] = '\0';
  strncpy(wifiParams->_wifiPassword, password.c_str(), sizeof(wifiParams->_wifiPassword) - 1);
  wifiParams->_wifiPassword[sizeof(wifiParams->_wifiPassword) - 1] = '\0';
  iotWebConf.saveConfig();
  srv->send(200, "application/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

// ==================== Storage APIs ====================

void handle_storage_status()
{
  log_v("handle_storage_status");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  String json = "{";
  json += "\"initialized\":" + String(storage.isInitialized() ? "true" : "false") + ",";
  json += "\"mounted\":" + String(storage.isMounted() ? "true" : "false") + ",";
  json += "\"enabled\":" + String(storage.isEnabled() ? "true" : "false") + ",";
  json += "\"recording\":" + String(recordingDesired ? "true" : "false") + ",";
  json += "\"total\":" + String(storage.getTotalBytes()) + ",";
  json += "\"used\":" + String(storage.getUsedBytes()) + ",";
  json += "\"free\":" + String(storage.getFreeBytes()) + ",";
  json += "\"snapshotCount\":" + String(storage.getSnapshotCount());
  json += "}";
  srv->send(200, "application/json", json);
}

void handle_snapshot_save()
{
  log_v("handle_snapshot_save");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  if (!storage.isInitialized() || !storage.isEnabled())
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"Storage not enabled\"}");
    return;
  }
  if (camera_init_result != ESP_OK)
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"Camera not initialized\"}");
    return;
  }
  for (auto i = 0; i < CAMERA_CONFIG_FB_COUNT; i++)
    cam.run();
  auto fb_len = cam.getSize();
  auto fb = (const char *)cam.getfb();
  if (fb == nullptr)
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"Unable to obtain frame buffer\"}");
    return;
  }
  if (storage.saveSnapshot((const uint8_t *)fb, fb_len))
    srv->send(200, "application/json", "{\"success\":true,\"count\":" + String(storage.getSnapshotCount()) + "}");
  else
    srv->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save snapshot\"}");
}

void handle_video_control()
{
  log_v("handle_video_control");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");
  if (!storage.isInitialized() || !storage.isEnabled())
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"Storage not enabled\"}");
    return;
  }
  bool start = srv->hasArg("action") && srv->arg("action") == "start";
  if (start && !recordingDesired && param_video_enabled.value())
  {
    recordingDesired = true;
    rec->enqueueControl(true);
    srv->send(200, "application/json", "{\"success\":true,\"recording\":true}");
  }
  else if (!start && recordingDesired)
  {
    recordingDesired = false;
    rec->enqueueControl(false);
    srv->send(200, "application/json", "{\"success\":true,\"recording\":false}");
  }
  else
  {
    srv->send(200, "application/json", "{\"success\":true,\"recording\":" + String(recordingDesired ? "true" : "false") + "}");
  }
}

// ==================== loop-time helper ====================

void handle_time_sync()
{
  log_v("handle_time_sync");
  srv->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  srv->sendHeader("Content-Type", "application/json");

  if (!srv->hasArg("timestamp"))
  {
    srv->send(400, "application/json", "{\"success\":false,\"error\":\"timestamp is required\"}");
    return;
  }

  long timestamp = srv->arg("timestamp").toInt();
  int timezone_minutes = 0;
  if (srv->hasArg("timezone"))
    timezone_minutes = srv->arg("timezone").toInt();

  struct timeval tv;
  tv.tv_sec = timestamp;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  char strftime_buf[64];
  struct tm timeinfo;
  localtime_r(&tv.tv_sec, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

  log_i("Time synchronized: %s (timezone: %d min)", strftime_buf, timezone_minutes);
  srv->send(200, "application/json", "{\"success\":true,\"time\":\"" + String(strftime_buf) + "\"}");
}

void handle_video_recording_loop()
{
  static unsigned long last_video_frame = 0;
  if (!recordingDesired || camera_init_result != ESP_OK || !rec)
    return;
  auto now = millis();
  if (now - last_video_frame < param_frame_duration.value())
    return;
  last_video_frame = now;
  cam.run();
  auto fb = (const uint8_t *)cam.getfb();
  auto len = cam.getSize();
  if (!fb || !len)
    return;
  uint8_t *copy = (uint8_t *)ps_malloc(len);
  if (!copy)
    return;
  memcpy(copy, fb, len);
  if (!rec->enqueueFrame(copy, len))
    free(copy);
}