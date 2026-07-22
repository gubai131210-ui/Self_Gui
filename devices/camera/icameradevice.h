#ifndef ICAMERADEVICE_H
#define ICAMERADEVICE_H

#include "foundation/selterror.h"
#include "vision/model/visionimage.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace Selt {

struct CameraDeviceInfo
{
    QString id;
    QString displayName;
    QString backend; // opencv, vendor-plugin, replay, file
    int width{0};
    int height{0};
    double fps{0.0};
    QString pixelFormat;
    QString serialNumber;
    QStringList capabilities; // grab, softTrigger, continuous, ...
};

class ICameraDevice
{
public:
    virtual ~ICameraDevice() = default;
    virtual CameraDeviceInfo info() const = 0;
    virtual Error open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual Error grab(VisionImage &out) = 0;
};

class ITriggerSource
{
public:
    virtual ~ITriggerSource() = default;
    virtual Error softTrigger() = 0;
};

/// Probe OpenCV camera indices that can open (best-effort; may be slow).
QVector<CameraDeviceInfo> probeOpenCvCameras(int maxIndex = 4);

/// OpenCV camera capture. Implementation keeps VideoCapture out of the public header
/// so videoio is an optional link dependency.
class OpenCvVideoCaptureDevice : public ICameraDevice, public ITriggerSource
{
public:
    explicit OpenCvVideoCaptureDevice(int index = 0);
    ~OpenCvVideoCaptureDevice() override;
    CameraDeviceInfo info() const override;
    Error open() override;
    void close() override;
    bool isOpen() const override;
    Error grab(VisionImage &out) override;
    Error softTrigger() override;

private:
    struct CaptureState;
    int m_index{0};
    bool m_open{false};
    std::unique_ptr<CaptureState> m_state;
};

class DirectoryReplayDevice : public ICameraDevice, public ITriggerSource
{
public:
    explicit DirectoryReplayDevice(const QString &directory);
    CameraDeviceInfo info() const override;
    Error open() override;
    void close() override;
    bool isOpen() const override;
    Error grab(VisionImage &out) override;
    Error softTrigger() override;

private:
    QString m_directory;
    QStringList m_files;
    int m_cursor{0};
    bool m_open{false};
};

/// Deterministic stub camera for lifecycle / disconnect / trigger tests (no hardware).
class StubCameraDevice : public ICameraDevice, public ITriggerSource
{
public:
    explicit StubCameraDevice(int width = 64, int height = 48);
    CameraDeviceInfo info() const override;
    Error open() override;
    void close() override;
    bool isOpen() const override;
    Error grab(VisionImage &out) override;
    Error softTrigger() override;

    void setFailNextGrab(bool fail) { m_failNextGrab = fail; }
    void setDisconnected(bool disconnected) { m_disconnected = disconnected; }
    qint64 grabCount() const { return m_grabCount; }

private:
    int m_width{64};
    int m_height{48};
    bool m_open{false};
    bool m_failNextGrab{false};
    bool m_disconnected{false};
    qint64 m_grabCount{0};
    bool m_triggered{false};
};

} // namespace Selt

#endif
