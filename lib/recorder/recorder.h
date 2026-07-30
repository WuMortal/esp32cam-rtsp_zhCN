#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class Storage;

struct RecordItem
{
  bool isFrame;
  bool start;
  uint8_t *data;
  size_t len;
};

class Recorder
{
public:
  Recorder(Storage &storage, volatile bool &recordingDesired, int coreId = 1);
  ~Recorder();

  bool begin();
  void stop();

  void enqueueControl(bool start);
  bool enqueueFrame(uint8_t *data, size_t len);

  bool isStarted() const { return queue_ != nullptr; }

private:
  static void taskFunc(void *arg);
  void runTask();

  Storage &storage_;
  volatile bool &recordingDesired_;
  int coreId_;

  QueueHandle_t queue_ = nullptr;
  TaskHandle_t task_ = nullptr;

  static constexpr int QUEUE_LEN = 4;
  static constexpr int STACK_SIZE = 8192;
};
