#ifndef INPUTSOURCESERVICE_H
#define INPUTSOURCESERVICE_H

#include "devices/camera/icameradevice.h"
#include "vision/runtime/runtimediagnostic.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <memory>

namespace Selt {

/// Application-facing input source lifecycle (camera / replay / file).
/// UI only connects signals; does not own OpenCV types.
class InputSourceService : public QObject
{
    Q_OBJECT
public:
    enum class SourceKind {
        None,
        Camera,
        DirectoryReplay,
        ImageFile
    };

    explicit InputSourceService(QObject *parent = nullptr);

    SourceKind kind() const { return m_kind; }
    bool isOpen() const;
    QString sourceDescription() const;
    QString deviceId() const;
    qint64 frameCount() const { return m_frameCount; }
    qint64 lastFrameId() const { return m_lastFrameId; }
    QDateTime lastGrabbedAt() const { return m_lastGrabbedAt; }
    CameraDeviceInfo deviceInfo() const;
    QVector<RuntimeDiagnostic> diagnostics() const { return m_diagnostics; }

    Error openCamera(int index);
    Error openDirectoryReplay(const QString &directory);
    Error openImageFile(const QString &path);
    Error openStubCamera();
    void close();
    Error grab(VisionImage &out);
    Error softTrigger();

signals:
    void sourceChanged();
    void diagnosticsChanged(const QVector<RuntimeDiagnostic> &diags);
    void frameReady();

private:
    void setDiagnostic(const RuntimeDiagnostic &diag);
    void clearDiagnostics();

    SourceKind m_kind{SourceKind::None};
    std::unique_ptr<ICameraDevice> m_device;
    QVector<RuntimeDiagnostic> m_diagnostics;
    qint64 m_frameCount{0};
    qint64 m_lastFrameId{0};
    QDateTime m_lastGrabbedAt;
};

class ImageFileDevice : public ICameraDevice
{
public:
    explicit ImageFileDevice(const QString &path);
    CameraDeviceInfo info() const override;
    Error open() override;
    void close() override;
    bool isOpen() const override;
    Error grab(VisionImage &out) override;

private:
    QString m_path;
    VisionImage m_image;
    bool m_open{false};
};

} // namespace Selt

#endif // INPUTSOURCESERVICE_H
