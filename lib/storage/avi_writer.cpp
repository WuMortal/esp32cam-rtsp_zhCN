#include "avi_writer.h"
#include <vector>

AviWriter::AviWriter()
    : width_(0), height_(0), fps_(10), frameCount_(0), moviStart_(0), fileSize_(0)
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

    file_ = SD_MMC.open(filename, FILE_WRITE);
    if (!file_)
        return false;

    moviStart_ = 224;
    fileSize_ = moviStart_ + 12;

    writeFourCC("RIFF");
    writeDword(0);
    writeFourCC("AVI ");

    writeFourCC("LIST");
    writeDword(200);
    writeFourCC("hdrl");

    writeFourCC("avih");
    writeDword(56);
    writeDword(1000000 / fps);
    writeDword(0);
    writeDword(0);
    writeDword(0x10);
    writeDword(0);
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
    writeDword(116);
    writeFourCC("strl");

    writeFourCC("strh");
    writeDword(56);
    writeFourCC("vids");
    writeFourCC("MJPG");
    writeDword(0);
    writeDword(0);
    writeWord(0);
    writeWord(0);
    writeDword(0);
    writeDword(fps_);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);

    writeFourCC("strf");
    writeDword(40);
    writeDword(40);
    writeDword(width_);
    writeDword(height_);
    writeWord(1);
    writeWord(24);
    writeFourCC("MJPG");
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);
    writeDword(0);

    writeFourCC("LIST");
    writeDword(0);
    writeFourCC("movi");

    return true;
}

bool AviWriter::writeFrame(const uint8_t *data, size_t len)
{
    if (!file_)
        return false;

    uint32_t offset = fileSize_ - moviStart_;
    frameOffsets_.push_back(offset);

    writeFourCC("00dc");
    writeDword(len);
    file_.write(data, len);
    if (len % 2)
        file_.write((uint8_t)0);

    fileSize_ += 8 + len + (len % 2);
    frameCount_++;
    return true;
}

void AviWriter::close()
{
    if (!file_)
        return;

    uint32_t idxSize = frameOffsets_.size() * 16;
    uint32_t totalSize = fileSize_ + 8 + idxSize;

    writeFourCC("idx1");
    writeDword(idxSize);

    for (size_t i = 0; i < frameOffsets_.size(); i++)
    {
        writeFourCC("00dc");
        writeDword(0x10);
        writeDword(frameOffsets_[i]);
    }

    uint32_t moviSize = fileSize_ - moviStart_ - 8;

    file_.seek(4);
    writeDword(totalSize - 8);

    file_.seek(moviStart_ - 4);
    writeDword(moviSize);

    file_.close();
}