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
    , videoWidth_(640)
    , videoHeight_(480)
    , videoFps_(10)
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
    static uint32_t sequence = 0;
    sequence++;

    struct timeval tv;
    gettimeofday(&tv, nullptr);

    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);

    char buffer[80];
    snprintf(buffer, sizeof(buffer), "/%s_%04d%02d%02d_%02d%02d%02d_%06lu_%lu%s",
             prefix,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             (unsigned long)tv.tv_usec,
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

    if (recording_ && aviWriter_.isOpen())
    {
        bool written = aviWriter_.writeFrame(data, len);
        videoFileSize_ += len;

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

        return written;
    }

    return false;
}

bool Storage::openVideoFile(int width, int height, int fps)
{
    if (!mounted_ || !enabled_)
    {
        if (!begin())
            return false;
    }

    if (!ensureDirectory(basePath_))
        return false;

    videoWidth_ = width;
    videoHeight_ = height;
    videoFps_ = fps;

    String fileName = generateFileName("vid", ".avi");
    if (!aviWriter_.open(fileName.c_str(), width, height, fps))
    {
        log_e("Failed to open video file: %s", fileName.c_str());
        return false;
    }

    videoStartTime_ = millis();
    videoFileSize_ = 0;
    log_i("Video file opened: %s (%dx%d @ %dfps)", fileName.c_str(), width, height, fps);
    return true;
}

bool Storage::writeVideoFrame(const uint8_t *data, size_t len)
{
    if (!recording_ || !aviWriter_.isOpen())
    {
        return false;
    }

    if (data == nullptr || len == 0)
        return false;

    bool written = aviWriter_.writeFrame(data, len);
    videoFileSize_ += len;

    if (videoMaxFileSize_ > 0 && videoFileSize_ >= videoMaxFileSize_)
    {
        log_i("Video file size limit reached");
        closeVideoFile();
        if (!openVideoFile(videoWidth_, videoHeight_, videoFps_))
            recording_ = false;
    }

    unsigned long elapsed = millis() - videoStartTime_;
    if (videoMaxDuration_ > 0 && elapsed >= videoMaxDuration_)
    {
        log_i("Video duration limit reached");
        closeVideoFile();
        if (!openVideoFile(videoWidth_, videoHeight_, videoFps_))
            recording_ = false;
    }

    return written;
}

void Storage::closeVideoFile()
{
    if (aviWriter_.isOpen())
    {
        unsigned long elapsedMs = millis() - videoStartTime_;
        int frameCount = aviWriter_.getFrameCount();
        if (elapsedMs > 0 && frameCount > 0) {
            int realFps = (int)((frameCount * 1000UL) / elapsedMs);
            log_i("Real recording framerate: %d fps (%d frames in %lu ms)", realFps, frameCount, elapsedMs);
            if (realFps > 0 && realFps <= 60) {
                aviWriter_.updateFps(realFps);
            }
        }
        aviWriter_.close();
        log_i("Video file closed");
    }
}

void Storage::setVideoRecording(bool record)
{
    if (record && !recording_)
    {
        recording_ = true;
        if (!openVideoFile(videoWidth_, videoHeight_, videoFps_))
        {
            recording_ = false;
            log_e("Failed to start video recording");
        }
    }
    else if (!record && recording_)
    {
        closeVideoFile();
        recording_ = false;
    }
}

void Storage::listFiles(std::vector<FileInfo> &files)
{
    files.clear();
    if (!mounted_ || !enabled_)
        return;

    File root = SD_MMC.open(basePath_);
    if (!root || !root.isDirectory())
        return;

    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            FileInfo info;
            String fullName = file.name();
            int lastSlash = fullName.lastIndexOf('/');
            if (lastSlash >= 0) {
                info.name = fullName.substring(lastSlash + 1);
            } else {
                info.name = fullName;
            }
            info.path = String(basePath_) + "/" + info.name;
            info.size = file.size();
            info.isVideo = info.name.endsWith(".avi");
            info.lastWrite = file.getLastWrite();
            files.push_back(info);
        }
        file = root.openNextFile();
    }
}

bool Storage::serveFile(const String &path, WebServer &server)
{
    if (!mounted_ || !enabled_)
    {
        server.send(404, "text/plain", "Storage not available");
        return false;
    }

    String cleanPath = path;
    int queryPos = cleanPath.indexOf('?');
    if (queryPos >= 0) {
        cleanPath = cleanPath.substring(0, queryPos);
    }

    String filePath;
    if (cleanPath.startsWith("/files/"))
    {
        String relativePath = cleanPath.substring(7);
        filePath = String(basePath_) + "/" + relativePath;
    }
    else
    {
        filePath = cleanPath;
    }

    if (!SD_MMC.exists(filePath))
    {
        server.send(404, "text/plain", "File not found");
        return false;
    }

    File file = SD_MMC.open(filePath, FILE_READ);
    if (!file)
    {
        server.send(500, "text/plain", "Failed to open file");
        return false;
    }

    String contentType = "application/octet-stream";
    if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg"))
        contentType = "image/jpeg";
    else if (filePath.endsWith(".avi"))
        contentType = "video/x-msvideo";
    else if (filePath.endsWith(".mjpeg"))
        contentType = "video/x-mjpeg";

    bool download = path.indexOf("download=1") >= 0;
    server.sendHeader("Cache-Control", "no-cache");
    if (download) {
        String filename = filePath.substring(filePath.lastIndexOf('/') + 1);
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    }
    server.setContentLength(file.size());
    server.send(200, contentType, "");

    const size_t bufSize = 1024;
    uint8_t buf[bufSize];
    size_t remaining = file.size();
    while (remaining > 0 && server.client().connected())
    {
        size_t toRead = (remaining < bufSize) ? remaining : bufSize;
        size_t bytesRead = file.read(buf, toRead);
        if (bytesRead == 0)
            break;
        server.sendContent_P((const char *)buf, bytesRead);
        remaining -= bytesRead;
        yield();
    }

    file.close();
    return true;
}