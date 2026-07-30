#include <Arduino.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <driver/i2c.h>
#include <ESPmDNS.h>
#include <web_handlers.h>
#include <app_globals.h>

// Parameter groups
iotwebconf::ParameterGroup param_group_camera("camera", "摄像头设置");
iotwebconf::UIntTParameter<unsigned long> param_frame_duration = iotwebconf::Builder<iotwebconf::UIntTParameter<unsigned long>>("fd").label("帧间隔 (ms)").defaultValue(DEFAULT_FRAME_DURATION).min(10).build();
iotwebconf::SelectTParameter<sizeof(frame_sizes[0])> param_frame_size = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(frame_sizes[0])>>("fs").label("分辨率").optionValues((const char *)&frame_sizes).optionNames((const char *)&frame_sizes).optionCount(sizeof(frame_sizes) / sizeof(frame_sizes[0])).nameLength(sizeof(frame_sizes[0])).defaultValue(DEFAULT_FRAME_SIZE).build();
iotwebconf::UIntTParameter<byte> param_jpg_quality = iotwebconf::Builder<iotwebconf::UIntTParameter<byte>>("q").label("JPEG 质量").defaultValue(DEFAULT_JPEG_QUALITY).min(1).max(100).build();
iotwebconf::IntTParameter<int> param_brightness = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("b").label("亮度").defaultValue(DEFAULT_BRIGHTNESS).min(-2).max(2).build();
iotwebconf::IntTParameter<int> param_contrast = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("c").label("对比度").defaultValue(DEFAULT_CONTRAST).min(-2).max(2).build();
iotwebconf::IntTParameter<int> param_saturation = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("s").label("饱和度").defaultValue(DEFAULT_SATURATION).min(-2).max(2).build();
iotwebconf::SelectTParameter<sizeof(camera_effects[0])> param_special_effect = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_effects[0])>>("e").label("特效").optionValues((const char *)&camera_effects).optionNames((const char *)&camera_effects).optionCount(sizeof(camera_effects) / sizeof(camera_effects[0])).nameLength(sizeof(camera_effects[0])).defaultValue(DEFAULT_EFFECT).build();
iotwebconf::CheckboxTParameter param_whitebal = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wb").label("白平衡").defaultValue(DEFAULT_WHITE_BALANCE).build();
iotwebconf::CheckboxTParameter param_awb_gain = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("awbg").label("自动白平衡增益").defaultValue(DEFAULT_WHITE_BALANCE_GAIN).build();
iotwebconf::SelectTParameter<sizeof(camera_wb_modes[0])> param_wb_mode = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_wb_modes[0])>>("wbm").label("白平衡模式").optionValues((const char *)&camera_wb_modes).optionNames((const char *)&camera_wb_modes).optionCount(sizeof(camera_wb_modes) / sizeof(camera_wb_modes[0])).nameLength(sizeof(camera_wb_modes[0])).defaultValue(DEFAULT_WHITE_BALANCE_MODE).build();
iotwebconf::CheckboxTParameter param_exposure_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("ec").label("曝光控制").defaultValue(DEFAULT_EXPOSURE_CONTROL).build();
iotwebconf::CheckboxTParameter param_aec2 = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("aec2").label("自动曝光 (DSP)").defaultValue(DEFAULT_AEC2).build();
iotwebconf::IntTParameter<int> param_ae_level = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("ael").label("自动曝光级别").defaultValue(DEFAULT_AE_LEVEL).min(-2).max(2).build();
iotwebconf::IntTParameter<int> param_aec_value = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("aecv").label("手动曝光值").defaultValue(DEFAULT_AEC_VALUE).min(9).max(1200).build();
iotwebconf::CheckboxTParameter param_gain_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("gc").label("增益控制").defaultValue(DEFAULT_GAIN_CONTROL).build();
iotwebconf::IntTParameter<int> param_agc_gain = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("agcg").label("AGC 增益").defaultValue(DEFAULT_AGC_GAIN).min(0).max(30).build();
iotwebconf::SelectTParameter<sizeof(camera_gain_ceilings[0])> param_gain_ceiling = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_gain_ceilings[0])>>("gcl").label("自动增益上限").optionValues((const char *)&camera_gain_ceilings).optionNames((const char *)&camera_gain_ceilings).optionCount(sizeof(camera_gain_ceilings) / sizeof(camera_gain_ceilings[0])).nameLength(sizeof(camera_gain_ceilings[0])).defaultValue(DEFAULT_GAIN_CEILING).build();
iotwebconf::CheckboxTParameter param_bpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("bpc").label("黑像素校正").defaultValue(DEFAULT_BPC).build();
iotwebconf::CheckboxTParameter param_wpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wpc").label("白像素校正").defaultValue(DEFAULT_WPC).build();
iotwebconf::CheckboxTParameter param_raw_gma = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("rg").label("Gamma 校正").defaultValue(DEFAULT_RAW_GAMMA).build();
iotwebconf::CheckboxTParameter param_lenc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("lenc").label("镜头校正").defaultValue(DEFAULT_LENC).build();
iotwebconf::CheckboxTParameter param_hmirror = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("hm").label("水平镜像").defaultValue(DEFAULT_HORIZONTAL_MIRROR).build();
iotwebconf::CheckboxTParameter param_vflip = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("vm").label("垂直翻转").defaultValue(DEFAULT_VERTICAL_MIRROR).build();
iotwebconf::CheckboxTParameter param_dcw = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("dcw").label("缩小启用").defaultValue(DEFAULT_DCW).build();
iotwebconf::CheckboxTParameter param_colorbar = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("cb").label("彩色条纹").defaultValue(DEFAULT_COLORBAR).build();

iotwebconf::ParameterGroup param_group_storage("storage", "存储设置");
iotwebconf::CheckboxTParameter param_storage_enabled = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("ste").label("启用存储").defaultValue(DEFAULT_STORAGE_ENABLED).build();
iotwebconf::TextTParameter<sizeof(DEFAULT_STORAGE_PATH)> param_storage_path = iotwebconf::Builder<iotwebconf::TextTParameter<sizeof(DEFAULT_STORAGE_PATH)>>("stp").label("存储路径").defaultValue(DEFAULT_STORAGE_PATH).build();
iotwebconf::CheckboxTParameter param_video_enabled = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("ve").label("启用录像").defaultValue(DEFAULT_VIDEO_ENABLED).build();
iotwebconf::UIntTParameter<unsigned long> param_video_duration = iotwebconf::Builder<iotwebconf::UIntTParameter<unsigned long>>("vd").label("录像时长 (ms)").defaultValue(DEFAULT_VIDEO_DURATION).min(1000).build();
iotwebconf::UIntTParameter<unsigned long> param_video_max_size = iotwebconf::Builder<iotwebconf::UIntTParameter<unsigned long>>("vms").label("单文件大小 (KB)").defaultValue(DEFAULT_VIDEO_MAX_SIZE).min(0).build();

// Global objects
OV2640 cam;
DNSServer dnsServer;
WebServer web_server(80);
std::unique_ptr<rtsp_server> camera_server;
esp_err_t camera_init_result;
volatile bool recordingDesired = false;
Recorder *recorder = nullptr;

auto thingName = String(WIFI_SSID) + "-" + String(ESP.getEfuseMac(), 16);
IotWebConf iotWebConf(thingName.c_str(), &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

// ==================== Camera ====================

esp_err_t initialize_camera()
{
  log_v("initialize_camera");
  log_i("Frame size: %s", param_frame_size.value());
  auto frame_size = lookup_frame_size(param_frame_size.value());
  log_i("JPEG quality: %d", param_jpg_quality.value());
  log_i("Frame duration: %d ms", param_frame_duration.value());
  const camera_config_t camera_config = {
      .pin_pwdn = CAMERA_CONFIG_PIN_PWDN,
      .pin_reset = CAMERA_CONFIG_PIN_RESET,
      .pin_xclk = CAMERA_CONFIG_PIN_XCLK,
      .pin_sccb_sda = CAMERA_CONFIG_PIN_SCCB_SDA,
      .pin_sccb_scl = CAMERA_CONFIG_PIN_SCCB_SCL,
      .pin_d7 = CAMERA_CONFIG_PIN_Y9,
      .pin_d6 = CAMERA_CONFIG_PIN_Y8,
      .pin_d5 = CAMERA_CONFIG_PIN_Y7,
      .pin_d4 = CAMERA_CONFIG_PIN_Y6,
      .pin_d3 = CAMERA_CONFIG_PIN_Y5,
      .pin_d2 = CAMERA_CONFIG_PIN_Y4,
      .pin_d1 = CAMERA_CONFIG_PIN_Y3,
      .pin_d0 = CAMERA_CONFIG_PIN_Y2,
      .pin_vsync = CAMERA_CONFIG_PIN_VSYNC,
      .pin_href = CAMERA_CONFIG_PIN_HREF,
      .pin_pclk = CAMERA_CONFIG_PIN_PCLK,
      .xclk_freq_hz = CAMERA_CONFIG_CLK_FREQ_HZ,
      .ledc_timer = CAMERA_CONFIG_LEDC_TIMER,
      .ledc_channel = CAMERA_CONFIG_LEDC_CHANNEL,
      .pixel_format = PIXFORMAT_JPEG,
      .frame_size = frame_size,
      .jpeg_quality = param_jpg_quality.value(),
      .fb_count = CAMERA_CONFIG_FB_COUNT,
      .fb_location = CAMERA_CONFIG_FB_LOCATION,
      .grab_mode = CAMERA_GRAB_LATEST,
#if CONFIG_CAMERA_CONVERTER_ENABLED
      conv_mode = CONV_DISABLE,
#endif
      .sccb_i2c_port = SCCB_I2C_PORT};
  return cam.init(camera_config);
}

void update_camera_settings()
{
  auto camera = esp_camera_sensor_get();
  if (camera == nullptr)
  {
    log_e("Unable to get camera sensor");
    return;
  }
  camera->set_framesize(camera, lookup_frame_size(param_frame_size.value()));
  camera->set_brightness(camera, param_brightness.value());
  camera->set_contrast(camera, param_contrast.value());
  camera->set_saturation(camera, param_saturation.value());
  camera->set_special_effect(camera, lookup_camera_effect(param_special_effect.value()));
  camera->set_whitebal(camera, param_whitebal.value());
  camera->set_awb_gain(camera, param_awb_gain.value());
  camera->set_wb_mode(camera, lookup_camera_wb_mode(param_wb_mode.value()));
  camera->set_exposure_ctrl(camera, param_exposure_ctrl.value());
  camera->set_aec2(camera, param_aec2.value());
  camera->set_ae_level(camera, param_ae_level.value());
  camera->set_aec_value(camera, param_aec_value.value());
  camera->set_gain_ctrl(camera, param_gain_ctrl.value());
  camera->set_agc_gain(camera, param_agc_gain.value());
  camera->set_gainceiling(camera, lookup_camera_gainceiling(param_gain_ceiling.value()));
  camera->set_bpc(camera, param_bpc.value());
  camera->set_wpc(camera, param_wpc.value());
  camera->set_raw_gma(camera, param_raw_gma.value());
  camera->set_lenc(camera, param_lenc.value());
  camera->set_hmirror(camera, param_hmirror.value());
  camera->set_vflip(camera, param_vflip.value());
  camera->set_dcw(camera, param_dcw.value());
  camera->set_colorbar(camera, param_colorbar.value());
}

// ==================== RTSP ====================

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  camera_server = std::unique_ptr<rtsp_server>(new rtsp_server(cam, param_frame_duration.value(), RTSP_PORT));
  MDNS.addService("rtsp", "tcp", RTSP_PORT);
}

void on_connected()
{
  log_v("on_connected");
  if (camera_init_result == ESP_OK)
    start_rtsp_server();
  else
    log_e("Not starting RTSP server: camera not initialized");
}

void on_config_saved()
{
  log_v("on_config_saved");
  update_camera_settings();
}

// ==================== setup / loop ====================

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
#ifdef CAMERA_POWER_GPIO
  pinMode(CAMERA_POWER_GPIO, OUTPUT);
  digitalWrite(CAMERA_POWER_GPIO, CAMERA_POWER_ON_LEVEL);
#endif
#ifdef USER_LED_GPIO
  pinMode(USER_LED_GPIO, OUTPUT);
  digitalWrite(USER_LED_GPIO, !USER_LED_ON_LEVEL);
#endif
#ifdef FLASH_LED_GPIO
  pinMode(FLASH_LED_GPIO, OUTPUT);
  analogWriteResolution(8);
  analogWrite(FLASH_LED_GPIO, 0);
#endif
#ifdef ARDUINO_USB_CDC_ON_BOOT
  delay(5000);
#endif

  log_i("Core debug level: %d", CORE_DEBUG_LEVEL);
  log_i("CPU Freq: %d Mhz, %d core(s)", getCpuFrequencyMhz(), ESP.getChipCores());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("SDK version: %s", ESP.getSdkVersion());
  log_i("Board: %s", BOARD_NAME);
  log_i("Starting " APP_TITLE "...");

  if (CAMERA_CONFIG_FB_LOCATION == CAMERA_FB_IN_PSRAM && !psramInit())
    log_e("Failed to initialize PSRAM");

  param_group_camera.addItem(&param_frame_duration);
  param_group_camera.addItem(&param_frame_size);
  param_group_camera.addItem(&param_jpg_quality);
  param_group_camera.addItem(&param_brightness);
  param_group_camera.addItem(&param_contrast);
  param_group_camera.addItem(&param_saturation);
  param_group_camera.addItem(&param_special_effect);
  param_group_camera.addItem(&param_whitebal);
  param_group_camera.addItem(&param_awb_gain);
  param_group_camera.addItem(&param_wb_mode);
  param_group_camera.addItem(&param_exposure_ctrl);
  param_group_camera.addItem(&param_aec2);
  param_group_camera.addItem(&param_ae_level);
  param_group_camera.addItem(&param_aec_value);
  param_group_camera.addItem(&param_gain_ctrl);
  param_group_camera.addItem(&param_agc_gain);
  param_group_camera.addItem(&param_gain_ceiling);
  param_group_camera.addItem(&param_bpc);
  param_group_camera.addItem(&param_wpc);
  param_group_camera.addItem(&param_raw_gma);
  param_group_camera.addItem(&param_lenc);
  param_group_camera.addItem(&param_hmirror);
  param_group_camera.addItem(&param_vflip);
  param_group_camera.addItem(&param_dcw);
  param_group_camera.addItem(&param_colorbar);
  iotWebConf.addParameterGroup(&param_group_camera);

  param_group_storage.addItem(&param_storage_enabled);
  param_group_storage.addItem(&param_storage_path);
  param_group_storage.addItem(&param_video_enabled);
  param_group_storage.addItem(&param_video_duration);
  param_group_storage.addItem(&param_video_max_size);
  iotWebConf.addParameterGroup(&param_group_storage);

  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setConfigSavedCallback(on_config_saved);
  iotWebConf.setWifiConnectionCallback(on_connected);
#ifdef USER_LED_GPIO
  iotWebConf.setStatusPin(USER_LED_GPIO, USER_LED_ON_LEVEL);
#endif
  iotWebConf.init();

  // Sync storage settings from iotWebConf params (loaded from NVS)
  storage.setEnabled(param_storage_enabled.value());
  storage.setBasePath(param_storage_path.value());
  storage.setVideoMaxDuration(param_video_duration.value());
  storage.setVideoMaxFileSize(param_video_max_size.value() * 1024);
  if (param_storage_enabled.value())
    storage.begin();

  // Try to initialize camera 3 times
  for (auto i = 0; i < 3; i++)
  {
    camera_init_result = initialize_camera();
    if (camera_init_result == ESP_OK)
    {
      update_camera_settings();
      auto sensor = esp_camera_sensor_get();
      if (sensor)
      {
        framesize_t fs = sensor->status.framesize;
        int w = 640, h = 480;
        switch (fs)
        {
        case FRAMESIZE_QQVGA:  w = 160; h = 120; break;
        case FRAMESIZE_QCIF:   w = 176; h = 144; break;
        case FRAMESIZE_HQVGA:  w = 240; h = 176; break;
        case FRAMESIZE_240X240: w = 240; h = 240; break;
        case FRAMESIZE_QVGA:   w = 320; h = 240; break;
        case FRAMESIZE_CIF:    w = 400; h = 296; break;
        case FRAMESIZE_HVGA:   w = 480; h = 320; break;
        case FRAMESIZE_VGA:    w = 640; h = 480; break;
        case FRAMESIZE_SVGA:   w = 800; h = 600; break;
        case FRAMESIZE_XGA:    w = 1024; h = 768; break;
        case FRAMESIZE_HD:     w = 1280; h = 720; break;
        case FRAMESIZE_SXGA:   w = 1280; h = 1024; break;
        case FRAMESIZE_UXGA:   w = 1600; h = 1200; break;
        default: break;
        }
        storage.setVideoParams(w, h, 1000 / param_frame_duration.value());
        log_i("Video recording: %dx%d @ %dfps", w, h, 1000 / param_frame_duration.value());
      }
      break;
    }
    esp_camera_deinit();
    log_e("Failed to initialize camera. Error: 0x%0x. Frame size: %s, frame rate: %d ms, jpeg quality: %d", camera_init_result, param_frame_size.value(), param_frame_duration.value(), param_jpg_quality.value());
    delay(500);
  }

  // Create recorder instance (after storage is initialized)
  recorder = new Recorder(storage, recordingDesired, 1);

  // Register all HTTP routes
  registerWebHandlers(web_server, *recorder);

  // Start async recorder task (Core 1)
  recorder->begin();
}

void loop()
{
  iotWebConf.doLoop();
  if (camera_server)
    camera_server->doLoop();
  handle_video_recording_loop();
}