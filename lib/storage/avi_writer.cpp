#include "avi_writer.h"
#include <vector>

AviWriter::AviWriter()
    : width_(0), height_(0), fps_(10), frameCount_(0), moviStart_(0), fileSize_(0),
      avihOffset_(0), strhRateOffset_(0), strhLengthOffset_(0), moviSizeOffset_(0), riffSizeOffset_(0)
{
}

AviWriter::~AviWriter()
{
    close();
}

void AviWriter::writeDword(uint32_t value)
{
    file_.write((const uint8_t *)&value, 4);
}

void AviWriter::writeWord(uint16_t value)
{
    file_.write((const uint8_t *)&value, 2);
}

void AviWriter::writeFourCC(const char *cc)
{
    file_.write((const uint8_t *)cc, 4);
}

bool AviWriter::open(const char *filename, int width, int height, int fps)
{
    if (file_)
        close();

    width_ = width;
    height_ = height;
    fps_ = fps;
    frameCount_ = 0;
    frameOffsets_.clear();
    frameLengths_.clear();

    file_ = SD_MMC.open(filename, FILE_WRITE);
    if (!file_)
        return false;

    const uint32_t avihDataSize = 56;
    const uint32_t strhDataSize = 56;
    const uint32_t strfDataSize = 40;
    const uint32_t avihChunkSize = 8 + avihDataSize;
    const uint32_t strhChunkSize = 8 + strhDataSize;
    const uint32_t strfChunkSize = 8 + strfDataSize;
    const uint32_t strlListSize = 4 + strhChunkSize + strfChunkSize;
    const uint32_t hdrlListSize = 4 + avihChunkSize + 8 + strlListSize;
    moviStart_ = 12 + 8 + hdrlListSize + 12;
    fileSize_ = moviStart_;

    riffSizeOffset_ = 4;
    writeFourCC("RIFF");
    writeDword(0);
    writeFourCC("AVI ");

    writeFourCC("LIST");
    writeDword(hdrlListSize);
    writeFourCC("hdrl");

    avihOffset_ = file_.position();
    writeFourCC("avih");
    writeDword(avihDataSize);
    writeDword(1000000 / fps);
    writeDword(0);
    writeDword(0);
    writeDword(0x10);
    writeDword(0);
    writeDword(0);
    writeDword(1);
    writeDword(0);
    writeDword(width_);
    writeDword(height_);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);

    writeFourCC("LIST");
    writeDword(strlListSize);
    writeFourCC("strl");

    writeFourCC("strh");
    writeDword(strhDataSize);
    writeFourCC("vids");
    writeFourCC("MJPG");
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(1);
    strhRateOffset_ = file_.position();
    writeDword(fps_);
    writeDword(0);
    strhLengthOffset_ = file_.position();
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeWord(0);
    writeWord(0);
    writeWord(width_);
    writeWord(height_);

    writeFourCC("strf");
    writeDword(strfDataSize);
    writeDword(40);
    writeDword(width_);
    writeDword(height_);
    writeWord(1);
    writeWord(24);
    writeFourCC("MJPG");
    writeDword(width_ * height_ * 3);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);

    writeFourCC("LIST");
    moviSizeOffset_ = file_.position();
    writeDword(0);
    writeFourCC("movi");

    return true;
}

bool AviWriter::writeFrame(const uint8_t *data, size_t len)
{
    if (!file_)
        return false;

    uint32_t frameOffsetInMovi = fileSize_ - moviStart_;
    frameOffsets_.push_back(frameOffsetInMovi);
    frameLengths_.push_back((uint32_t)len);

    writeFourCC("00dc");
    writeDword((uint32_t)len);
    size_t written = file_.write(data, len);
    if (written != len) {
        log_e("Failed to write full frame");
        return false;
    }

    uint32_t paddedLen = (uint32_t)len;
    if (len % 2) {
        uint8_t pad = 0;
        file_.write(&pad, 1);
        paddedLen++;
    }

    fileSize_ += 8 + paddedLen;
    frameCount_++;
    return true;
}

void AviWriter::updateFps(int realFps)
{
    if (realFps <= 0) return;
    fps_ = realFps;

    if (!file_) return;

    size_t currentPos = file_.position();

    file_.seek(avihOffset_ + 8);
    writeDword(1000000 / fps_);

    file_.seek(strhRateOffset_);
    writeDword(fps_);

    file_.seek(currentPos);
}

void AviWriter::close()
{
    if (!file_)
        return;

    uint32_t moviDataSize = fileSize_ - moviStart_;
    uint32_t idxEntrySize = 16;
    uint32_t idxDataSize = frameCount_ * idxEntrySize;
    uint32_t idxChunkSize = 8 + idxDataSize;
    uint32_t totalFileSize = fileSize_ + idxChunkSize;

    writeFourCC("idx1");
    writeDword(idxDataSize);

    for (int i = 0; i < frameCount_; i++) {
        writeFourCC("00dc");
        writeDword(0x10);
        writeDword(frameOffsets_[i]);
        writeDword(frameLengths_[i]);
    }

    file_.seek(riffSizeOffset_);
    writeDword(totalFileSize - 8);

    file_.seek(moviSizeOffset_);
    writeDword(moviDataSize);

    file_.seek(avihOffset_ + 24);
    writeDword(frameCount_);

    file_.seek(strhLengthOffset_);
    writeDword(frameCount_);

    file_.close();
}
