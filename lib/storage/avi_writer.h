#pragma once

#include <SD_MMC.h>
#include <Arduino.h>
#include <vector>

class AviWriter
{
public:
    AviWriter();
    ~AviWriter();

    bool open(const char *filename, int width, int height, int fps);
    void close();
    bool isOpen() const { return file_; }
    bool writeFrame(const uint8_t *data, size_t len);

    int getFrameCount() const { return frameCount_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getFps() const { return fps_; }
    void updateFps(int realFps);

private:
    File file_;
    int width_;
    int height_;
    int fps_;
    int frameCount_;
    uint32_t moviStart_;
    uint32_t fileSize_;
    std::vector<uint32_t> frameOffsets_;
    std::vector<uint32_t> frameLengths_;

    uint32_t avihOffset_;
    uint32_t strhRateOffset_;
    uint32_t strhLengthOffset_;
    uint32_t moviSizeOffset_;
    uint32_t riffSizeOffset_;

    void writeDword(uint32_t value);
    void writeWord(uint16_t value);
    void writeFourCC(const char *cc);
};