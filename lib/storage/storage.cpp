#include <storage.h>
#include <time.h>
#include <sys/time.h>

Storage storage;

Storage::Storage()
    : initialized_(false)
    , mounted_(false)
    , enabled_(false)
    , recording_(false)
    , snapshotCount_(0)
    , videoStartTime_(0)
    , videoMaxDuration_(60000)
    , videoMaxFileSize_(0)
    , videoFileSize_(0)
{
    mountPoint_ = "/sdcard";
    strcpy(basePath_, "/wigorcam/");
}

bool Storage::begin()
{
    if (initialized_ && enabled_)
        return true;

    if (!enabled_)
        return false;

    if (initialized_)
    {
        // Already initialized but disabled before, remount
        SD_MMC.end();
        initialized_ = false;
        mounted_ = false;
    }

    if (!SD_MMC.begin(mountPoint_.c_str(), true))
    {
        log_e("SD_MMC initialization failed (mountpoint=%s, mode=1bit)", mountPoint_.c_str());
        return false;
    }

    if (!getCardInfo())
    {
        log_e("Failed to get SD card info");
        SD_MMC.end();
        return false;
    }

    initialized_ = true;
    log_i("SD card initialized (SD_MMC 1-bit mode)");
    return true;
}

void Storage::end()
{
    if (!initialized_)
        return;

    closeVideoFile();
    SD_MMC.end();
    initialized_ = false;
    mounted_ = false;
    log_i("SD card unmounted");
}

bool Storage::getCardInfo()
{
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE)
    {
        log_e("No SD card attached");
        return false;
    }

    const char* typeNames[] = { "UNKNOWN", "MMC", "SDSC", "SDHC", "" };
    log_i("SD Card Type: %s", typeNames[cardType]);

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    log_i("SD Card Size: %llu MB", cardSize);

    mounted_ = true;
    return true;
}

uint64_t Storage::getTotalBytes()
{
    if (!mounted_)
        return 0;
    return SD_MMC.totalBytes();
}

uint64_t Storage::getUsedBytes()
{
    if (!mounted_)
        return 0;
    return SD_MMC.usedBytes();
}

uint64_t Storage::getFreeBytes()
{
    if (!mounted_)
        return 0;
    return getTotalBytes() - getUsedBytes();
}

void Storage::setBasePath(const char *path)
{
    strncpy(basePath_, path, sizeof(basePath_) - 1);
    basePath_[sizeof(basePath_) - 1] = '\0';
    if (basePath_[strlen(basePath_) - 1] == '/')
    {
        basePath_[strlen(basePath_) - 1] = '\0';
    }
}

bool Storage::ensureDirectory(const char *path)
{
    if (!mounted_)
        return false;

    if (!SD_MMC.exists(path))
    {
        if (!SD_MMC.mkdir(path))
        {
            log_e("Failed to create directory: %s", path);
            return false;
        }
    }
    return true;
}

String Storage::generateFileName(const char *prefix, const char *extension)
{
    // 时间戳可能未同步（无 SNTP），追加递增序号保证跨重启/同微秒不重名
    static uint32_t sequence = 0;
    sequence++;

    struct timeval tv;
    gettimeofday(&tv, nullptr);

    char buffer[80];
    snprintf(buffer, sizeof(buffer), "/%s_%lu_%06lu_%lu%s",
             prefix, (unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec,
             (unsigned long)sequence, extension);
    return String(basePath_) + String(buffer);
}

bool Storage::saveSnapshot(const uint8_t *fb_data, size_t fb_len)
{
    if (!mounted_ || !enabled_)
        return false;

    if (fb_data == nullptr || fb_len == 0)
        return false;

    if (!ensureDirectory(basePath_))
        return false;

    String fileName = generateFileName("img", ".jpg");
    File file = SD_MMC.open(fileName.c_str(), FILE_WRITE);
    if (!file)
    {
        log_e("Failed to open file for snapshot: %s", fileName.c_str());
        return false;
    }

    size_t written = file.write(fb_data, fb_len);
    file.close();

    if (written != fb_len)
    {
        log_e("Failed to write full snapshot");
        return false;
    }

    incrementSnapshotCount();
    log_i("Snapshot saved: %s (%d bytes)", fileName.c_str(), fb_len);
    return true;
}

bool Storage::saveJpegFrame(const uint8_t *data, size_t len)
{
    if (!mounted_ || !enabled_)
        return false;

    if (data == nullptr || len == 0)
        return false;

    if (recording_ && videoFile_)
    {
        size_t written = videoFile_.write(data, len);
        videoFileSize_ += written;

        if (videoMaxFileSize_ > 0 && videoFileSize_ >= videoMaxFileSize_)
        {
            log_i("Video file size limit reached, closing file");
            closeVideoFile();
        }

        unsigned long elapsed = millis() - videoStartTime_;
        if (videoMaxDuration_ > 0 && elapsed >= videoMaxDuration_)
        {
            log_i("Video duration limit reached, closing file");
            closeVideoFile();
        }

        return true;
    }

    return false;
}

bool Storage::openVideoFile()
{
    if (!mounted_ || !enabled_)
        return false;

    if (!ensureDirectory(basePath_))
        return false;

    String fileName = generateFileName("vid", ".mjpeg");
    videoFile_ = SD_MMC.open(fileName.c_str(), FILE_WRITE);
    if (!videoFile_)
    {
        log_e("Failed to open video file: %s", fileName.c_str());
        return false;
    }

    videoStartTime_ = millis();
    videoFileSize_ = 0;
    log_i("Video file opened: %s", fileName.c_str());
    return true;
}

bool Storage::writeVideoFrame(const uint8_t *data, size_t len)
{
    if (!recording_ || !videoFile_)
    {
        // 未在录像状态：不写帧，避免录像模式下分片失败误回退成快照
        return false;
    }

    if (data == nullptr || len == 0)
        return false;

    size_t written = videoFile_.write(data, len);
    videoFileSize_ += written;

    if (videoMaxFileSize_ > 0 && videoFileSize_ >= videoMaxFileSize_)
    {
        log_i("Video file size limit reached");
        closeVideoFile();
        if (!openVideoFile())
            recording_ = false;
    }

    unsigned long elapsed = millis() - videoStartTime_;
    if (videoMaxDuration_ > 0 && elapsed >= videoMaxDuration_)
    {
        log_i("Video duration limit reached");
        closeVideoFile();
        if (!openVideoFile())
            recording_ = false;
    }

    return written == len;
}

void Storage::closeVideoFile()
{
    if (videoFile_)
    {
        videoFile_.close();
        log_i("Video file closed");
    }
}

void Storage::setVideoRecording(bool record)
{
    if (record && !recording_)
    {
        recording_ = true;
        if (!openVideoFile())
        {
            recording_ = false;
            log_e("Failed to start video recording");
        }
    }
    else if (!record && recording_)
    {
        end();
    }
}