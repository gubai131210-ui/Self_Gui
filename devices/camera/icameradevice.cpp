#include "devices/camera/icameradevice.h"

#include "vision/algorithms/imageloader.h"

#include <QDir>

#if SELT_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

#if SELT_HAS_OPENCV && defined(SELT_HAS_OPENCV_VIDEOIO) && SELT_HAS_OPENCV_VIDEOIO
#include <opencv2/videoio.hpp>
#define SELT_USE_VIDEO_CAPTURE 1
#else
#define SELT_USE_VIDEO_CAPTURE 0
#endif

namespace Selt {

struct OpenCvVideoCaptureDevice::CaptureState
{
#if SELT_USE_VIDEO_CAPTURE
    cv::VideoCapture capture;
#endif
};

OpenCvVideoCaptureDevice::OpenCvVideoCaptureDevice(int index)
    : m_index(index)
    , m_state(std::make_unique<CaptureState>())
{
}

OpenCvVideoCaptureDevice::~OpenCvVideoCaptureDevice()
{
    close();
}

CameraDeviceInfo OpenCvVideoCaptureDevice::info() const
{
    CameraDeviceInfo info;
    info.id = QStringLiteral("opencv-cam-%1").arg(m_index);
    info.displayName = QStringLiteral("OpenCV Camera %1").arg(m_index);
    info.backend = QStringLiteral("opencv");
    info.capabilities = {QStringLiteral("grab"), QStringLiteral("softTrigger"), QStringLiteral("continuous")};
#if SELT_USE_VIDEO_CAPTURE
    if (m_open && m_state && m_state->capture.isOpened()) {
        info.width = static_cast<int>(m_state->capture.get(cv::CAP_PROP_FRAME_WIDTH));
        info.height = static_cast<int>(m_state->capture.get(cv::CAP_PROP_FRAME_HEIGHT));
        info.fps = m_state->capture.get(cv::CAP_PROP_FPS);
    }
#endif
    return info;
}

Error OpenCvVideoCaptureDevice::open()
{
#if SELT_USE_VIDEO_CAPTURE
    if (m_open)
        return Error::success();
    if (!m_state)
        m_state = std::make_unique<CaptureState>();
    m_state->capture.open(m_index);
    m_open = m_state->capture.isOpened();
    if (!m_open)
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("无法打开相机 %1").arg(m_index));
    return Error::success();
#elif SELT_HAS_OPENCV
    return Error::fail(ErrorCode::NotImplemented,
                       QStringLiteral("当前 OpenCV 未链接 videoio，相机不可用（可用目录回放/图片输入）"));
#else
    return Error::fail(ErrorCode::NotImplemented, QStringLiteral("OpenCV 未启用"));
#endif
}

void OpenCvVideoCaptureDevice::close()
{
#if SELT_USE_VIDEO_CAPTURE
    if (m_state && m_state->capture.isOpened())
        m_state->capture.release();
#endif
    m_open = false;
}

bool OpenCvVideoCaptureDevice::isOpen() const
{
    return m_open;
}

Error OpenCvVideoCaptureDevice::grab(VisionImage &out)
{
#if SELT_USE_VIDEO_CAPTURE
    if (!m_open || !m_state || !m_state->capture.isOpened())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("相机未打开"));
    cv::Mat frame;
    if (!m_state->capture.read(frame) || frame.empty())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("抓帧失败"));
    out = VisionImage(frame);
    return Error::success();
#else
    Q_UNUSED(out);
    return Error::fail(ErrorCode::NotImplemented,
                       QStringLiteral("当前 OpenCV 未链接 videoio，相机不可用"));
#endif
}

Error OpenCvVideoCaptureDevice::softTrigger()
{
    VisionImage unused;
    return grab(unused);
}

DirectoryReplayDevice::DirectoryReplayDevice(const QString &directory)
    : m_directory(directory)
{
}

CameraDeviceInfo DirectoryReplayDevice::info() const
{
    CameraDeviceInfo info;
    info.id = QStringLiteral("replay:%1").arg(m_directory);
    info.displayName = QStringLiteral("目录回放");
    info.backend = QStringLiteral("replay");
    info.capabilities = {QStringLiteral("grab"), QStringLiteral("softTrigger")};
    return info;
}

Error DirectoryReplayDevice::open()
{
    QDir dir(m_directory);
    if (!dir.exists())
        return Error::fail(ErrorCode::NotFound, QStringLiteral("目录不存在"));
    m_files = dir.entryList({QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.bmp"),
                             QStringLiteral("*.jpeg"), QStringLiteral("*.tif"), QStringLiteral("*.tiff")},
                            QDir::Files, QDir::Name);
    if (m_files.isEmpty())
        return Error::fail(ErrorCode::NotFound, QStringLiteral("目录中没有图片"));
    m_cursor = 0;
    m_open = true;
    return Error::success();
}

void DirectoryReplayDevice::close()
{
    m_open = false;
    m_cursor = 0;
}

bool DirectoryReplayDevice::isOpen() const
{
    return m_open;
}

Error DirectoryReplayDevice::grab(VisionImage &out)
{
    if (!m_open || m_files.isEmpty())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("回放未打开"));
    const QString path = QDir(m_directory).filePath(m_files.at(m_cursor));
    m_cursor = (m_cursor + 1) % m_files.size();
    QString error;
    if (!ImageLoader::load(path, out, &error))
        return Error::fail(ErrorCode::ExecutionFailed, error);
    return Error::success();
}

Error DirectoryReplayDevice::softTrigger()
{
    VisionImage unused;
    return grab(unused);
}

StubCameraDevice::StubCameraDevice(int width, int height)
    : m_width(width)
    , m_height(height)
{
}

CameraDeviceInfo StubCameraDevice::info() const
{
    CameraDeviceInfo info;
    info.id = QStringLiteral("stub-camera");
    info.displayName = QStringLiteral("Stub Camera");
    info.backend = QStringLiteral("vendor-plugin");
    info.width = m_width;
    info.height = m_height;
    info.fps = 30.0;
    info.pixelFormat = QStringLiteral("BGR8");
    info.serialNumber = QStringLiteral("STUB-001");
    info.capabilities = {QStringLiteral("grab"), QStringLiteral("softTrigger"), QStringLiteral("disconnect-sim")};
    return info;
}

Error StubCameraDevice::open()
{
    if (m_disconnected)
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("设备已断开"));
    m_open = true;
    m_triggered = true; // free-run by default
    return Error::success();
}

void StubCameraDevice::close()
{
    m_open = false;
}

bool StubCameraDevice::isOpen() const
{
    return m_open && !m_disconnected;
}

Error StubCameraDevice::grab(VisionImage &out)
{
    if (!isOpen())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("相机未打开或已断开"));
    if (m_failNextGrab) {
        m_failNextGrab = false;
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("抓帧超时"));
    }
    if (!m_triggered)
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("等待软触发"));
#if SELT_HAS_OPENCV
    cv::Mat frame(m_height, m_width, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::rectangle(frame, cv::Rect(10, 10, m_width / 2, m_height / 2), cv::Scalar(0, 255, 0), -1);
    out = VisionImage(frame);
#else
    Q_UNUSED(out);
    return Error::fail(ErrorCode::NotImplemented, QStringLiteral("OpenCV 未启用"));
#endif
    ++m_grabCount;
    m_triggered = false;
    return Error::success();
}

Error StubCameraDevice::softTrigger()
{
    if (!isOpen())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("软触发失败：设备未打开"));
    m_triggered = true;
    return Error::success();
}

QVector<CameraDeviceInfo> probeOpenCvCameras(int maxIndex)
{
    QVector<CameraDeviceInfo> list;
#if SELT_USE_VIDEO_CAPTURE
    for (int i = 0; i <= qMax(0, maxIndex); ++i) {
        cv::VideoCapture cap;
        if (!cap.open(i))
            continue;
        if (!cap.isOpened())
            continue;
        CameraDeviceInfo info;
        info.id = QStringLiteral("opencv-cam-%1").arg(i);
        info.displayName = QStringLiteral("OpenCV Camera %1").arg(i);
        info.backend = QStringLiteral("opencv");
        info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        info.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        info.fps = cap.get(cv::CAP_PROP_FPS);
        info.capabilities = {QStringLiteral("grab"), QStringLiteral("softTrigger"), QStringLiteral("continuous")};
        cap.release();
        list.append(info);
    }
#else
    Q_UNUSED(maxIndex);
#endif
    return list;
}

} // namespace Selt
