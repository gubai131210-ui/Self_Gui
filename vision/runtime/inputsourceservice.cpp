#include "vision/runtime/inputsourceservice.h"

#include "vision/algorithms/imageloader.h"

#include <QDateTime>

namespace Selt {

ImageFileDevice::ImageFileDevice(const QString &path)
    : m_path(path)
{
}

CameraDeviceInfo ImageFileDevice::info() const
{
    CameraDeviceInfo info;
    info.id = QStringLiteral("file:%1").arg(m_path);
    info.displayName = QStringLiteral("单张图片");
    info.backend = QStringLiteral("file");
    info.capabilities = {QStringLiteral("grab")};
    return info;
}

Error ImageFileDevice::open()
{
    QString error;
    if (!ImageLoader::load(m_path, m_image, &error))
        return Error::fail(ErrorCode::ExecutionFailed, error);
    m_open = true;
    return Error::success();
}

void ImageFileDevice::close()
{
    m_open = false;
    m_image = VisionImage{};
}

bool ImageFileDevice::isOpen() const
{
    return m_open;
}

Error ImageFileDevice::grab(VisionImage &out)
{
    if (!m_open || m_image.isEmpty())
        return Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("图片未打开"));
    out = m_image.clone();
    return Error::success();
}

InputSourceService::InputSourceService(QObject *parent)
    : QObject(parent)
{
}

bool InputSourceService::isOpen() const
{
    return m_device && m_device->isOpen();
}

QString InputSourceService::sourceDescription() const
{
    if (!m_device)
        return QStringLiteral("无输入源");
    return m_device->info().displayName;
}

QString InputSourceService::deviceId() const
{
    if (!m_device)
        return {};
    return m_device->info().id;
}

CameraDeviceInfo InputSourceService::deviceInfo() const
{
    if (!m_device)
        return {};
    return m_device->info();
}

void InputSourceService::setDiagnostic(const RuntimeDiagnostic &diag)
{
    m_diagnostics.append(diag);
    while (m_diagnostics.size() > 32)
        m_diagnostics.removeFirst();
    emit diagnosticsChanged(m_diagnostics);
}

void InputSourceService::clearDiagnostics()
{
    if (m_diagnostics.isEmpty())
        return;
    m_diagnostics.clear();
    emit diagnosticsChanged(m_diagnostics);
}

Error InputSourceService::openCamera(int index)
{
    close();
    clearDiagnostics();
    m_device = std::make_unique<OpenCvVideoCaptureDevice>(index);
    const Error err = m_device->open();
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("camera_open")));
        m_device.reset();
        m_kind = SourceKind::None;
        return err;
    }
    m_kind = SourceKind::Camera;
    emit sourceChanged();
    return Error::success();
}

Error InputSourceService::openDirectoryReplay(const QString &directory)
{
    close();
    clearDiagnostics();
    m_device = std::make_unique<DirectoryReplayDevice>(directory);
    const Error err = m_device->open();
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("replay_open")));
        m_device.reset();
        m_kind = SourceKind::None;
        return err;
    }
    m_kind = SourceKind::DirectoryReplay;
    emit sourceChanged();
    return Error::success();
}

Error InputSourceService::openImageFile(const QString &path)
{
    close();
    clearDiagnostics();
    m_device = std::make_unique<ImageFileDevice>(path);
    const Error err = m_device->open();
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("file_open")));
        m_device.reset();
        m_kind = SourceKind::None;
        return err;
    }
    m_kind = SourceKind::ImageFile;
    emit sourceChanged();
    return Error::success();
}

Error InputSourceService::openStubCamera()
{
    close();
    clearDiagnostics();
    m_device = std::make_unique<StubCameraDevice>();
    const Error err = m_device->open();
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("stub_open")));
        m_device.reset();
        m_kind = SourceKind::None;
        return err;
    }
    m_kind = SourceKind::Camera;
    emit sourceChanged();
    return Error::success();
}

void InputSourceService::close()
{
    if (m_device)
        m_device->close();
    m_device.reset();
    m_kind = SourceKind::None;
    m_frameCount = 0;
    m_lastFrameId = 0;
    m_lastGrabbedAt = {};
    clearDiagnostics();
    emit sourceChanged();
}

Error InputSourceService::grab(VisionImage &out)
{
    if (!m_device || !m_device->isOpen()) {
        const Error err = Error::fail(ErrorCode::ExecutionFailed, QStringLiteral("输入源未打开"));
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("grab")));
        return err;
    }
    const Error err = m_device->grab(out);
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("grab")));
        return err;
    }
    ++m_frameCount;
    m_lastFrameId = m_frameCount;
    m_lastGrabbedAt = QDateTime::currentDateTimeUtc();
    emit frameReady();
    return Error::success();
}

Error InputSourceService::softTrigger()
{
    auto *trigger = dynamic_cast<ITriggerSource *>(m_device.get());
    if (!trigger) {
        const Error err = Error::fail(ErrorCode::NotImplemented, QStringLiteral("当前输入源不支持软触发"));
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("soft_trigger")));
        return err;
    }
    const Error err = trigger->softTrigger();
    if (!err.ok()) {
        setDiagnostic(RuntimeDiagnostic::make(RuntimeFailureKind::Device, err.message,
                                               {}, QStringLiteral("soft_trigger")));
    }
    return err;
}

} // namespace Selt
