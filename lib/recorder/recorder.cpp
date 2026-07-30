#include "recorder.h"
#include <storage.h>

Recorder::Recorder(Storage &storage, volatile bool &recordingDesired, int coreId)
    : storage_(storage), recordingDesired_(recordingDesired), coreId_(coreId)
{
}

Recorder::~Recorder()
{
  stop();
}

bool Recorder::begin()
{
  if (queue_)
    return true;
  queue_ = xQueueCreate(QUEUE_LEN, sizeof(RecordItem));
  if (!queue_)
    return false;
  BaseType_t ok = xTaskCreatePinnedToCore(&Recorder::taskFunc, "recordTask",
                                          STACK_SIZE, this, 1, &task_, coreId_);
  if (ok != pdPASS)
  {
    vQueueDelete(queue_);
    queue_ = nullptr;
    task_ = nullptr;
    return false;
  }
  return true;
}

void Recorder::stop()
{
  if (task_)
  {
    vTaskDelete(task_);
    task_ = nullptr;
  }
  if (queue_)
  {
    // Drain any remaining frames to free PSRAM copies
    RecordItem item;
    while (xQueueReceive(queue_, &item, 0) == pdTRUE)
    {
      if (item.isFrame && item.data)
        free(item.data);
    }
    vQueueDelete(queue_);
    queue_ = nullptr;
  }
}

void Recorder::enqueueControl(bool start)
{
  if (!queue_)
    return;
  RecordItem item = {};
  item.isFrame = false;
  item.start = start;
  xQueueSend(queue_, &item, pdMS_TO_TICKS(100));
}

bool Recorder::enqueueFrame(uint8_t *data, size_t len)
{
  if (!queue_)
    return false;
  RecordItem item = {};
  item.isFrame = true;
  item.data = data;
  item.len = len;
  return xQueueSend(queue_, &item, 0) == pdTRUE;
}

void Recorder::taskFunc(void *arg)
{
  static_cast<Recorder *>(arg)->runTask();
}

void Recorder::runTask()
{
  RecordItem item;
  while (true)
  {
    if (xQueueReceive(queue_, &item, portMAX_DELAY) != pdTRUE)
      continue;
    if (item.isFrame)
    {
      storage_.writeVideoFrame(item.data, item.len);
      free(item.data);
    }
    else if (item.start)
    {
      storage_.setVideoRecording(true);
    }
    else
    {
      storage_.setVideoRecording(false);
    }
  }
}
