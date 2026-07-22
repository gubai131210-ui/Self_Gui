#include "devices/camera/icameradevice.h"

#include <QtTest>

class TestCameraLifecycle : public QObject
{
    Q_OBJECT

private slots:
    void stubOpenGrabClose();
    void softTriggerRequiredAfterGrab();
    void disconnectAndTimeout();
};

void TestCameraLifecycle::stubOpenGrabClose()
{
    Selt::StubCameraDevice cam;
    QVERIFY(cam.open().ok());
    QVERIFY(cam.isOpen());
    VisionImage frame;
    QVERIFY(cam.grab(frame).ok());
    QVERIFY(!frame.isEmpty());
    cam.close();
    QVERIFY(!cam.isOpen());
}

void TestCameraLifecycle::softTriggerRequiredAfterGrab()
{
    Selt::StubCameraDevice cam;
    QVERIFY(cam.open().ok());
    VisionImage frame;
    QVERIFY(cam.grab(frame).ok());
    QVERIFY(!cam.grab(frame).ok()); // needs soft trigger
    QVERIFY(cam.softTrigger().ok());
    QVERIFY(cam.grab(frame).ok());
}

void TestCameraLifecycle::disconnectAndTimeout()
{
    Selt::StubCameraDevice cam;
    QVERIFY(cam.open().ok());
    cam.setFailNextGrab(true);
    VisionImage frame;
    QVERIFY(!cam.grab(frame).ok());
    QVERIFY(cam.softTrigger().ok());
    QVERIFY(cam.grab(frame).ok());

    cam.setDisconnected(true);
    QVERIFY(!cam.isOpen());
    QVERIFY(!cam.softTrigger().ok());
}

QTEST_MAIN(TestCameraLifecycle)
#include "test_camera_lifecycle.moc"
