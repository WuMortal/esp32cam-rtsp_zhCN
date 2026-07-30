#pragma once

#include <Arduino.h>
#include <SD_MMC.h>
#include "avi_writer.h"

class Storage
{
public:
    Storage();

    bool begin();
    void end();
    bool isInitialized() const { return initialized_; }
    bool isMounted() const { return mounted_; }

    bool saveJpegFrame(const uint8_t *data, size_t len);
    bool saveSnapshot(const uint8_t *fb_data, size_t fb_len);

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setRecording(bool recording) { recording_ = recording; }
    bool isRecording() const { return recording_; }

    uint64_t getTotalBytes();
    uint64_t getUsedBytes();
    uint64_t getFreeBytes();

    String getMountPoint() const { return mountPoint_; }
    bool getCardInfo();

    void setBasePath(const char *path);
    const char* getBasePath() const { return basePath_; }

    bool isVideoRecording() const { return recording_; }
    void setVideoRecording(bool record);

    void closeVideoFile();
    bool writeVideoFrame(const uint8_t *data, size_t len);

    void setSnapshotCount(int count) { snapshotCount_ = count; }
    int getSnapshotCount() const { return snapshotCount_; }

    void incrementSnapshotCount() { snapshotCount_++; }
    void setVideoMaxDuration(unsigned long ms) { videoMaxDuration_ = ms; }
    unsigned long getVideoMaxDuration() const { return videoMaxDuration_; }
    void setVideoMaxFileSize(unsigned long size) { videoMaxFileSize_ = size; }
    unsigned long getVideoMaxFileSize() const { return videoMaxFileSize_; }
    unsigned long getVideoStartTime() const { return videoStartTime_; }

    void setVideoParams(int width, int height, int fps) { videoWidth_ = width; videoHeight_ = height; videoFps_ = fps; }

private:
    bool initialized_;
    bool mounted_;
    bool enabled_;
    bool recording_;

    String mountPoint_;
    char basePath_[64];

    AviWriter aviWriter_;
    int videoWidth_;
    int videoHeight_;
    int videoFps_;
    unsigned long videoStartTime_;
    unsigned long videoMaxDuration_;
    unsigned long videoMaxFileSize_;
    unsigned long videoFileSize_;

    int snapshotCount_;

    bool ensureDirectory(const char *path);
    String generateFileName(const char *prefix, const char *extension);
    bool openVideoFile(int width, int height, int fps);
};

extern Storage storage;