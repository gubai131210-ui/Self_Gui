#ifndef OPENCVGUARD_H
#define OPENCVGUARD_H

#include <QString>
#include <functional>
#include <opencv2/core.hpp>

namespace Selt {

/// Run an OpenCV-facing callable and convert cv::Exception into an error string.
inline bool runOpenCv(const std::function<void()> &fn, QString *errorMessage = nullptr)
{
    try {
        fn();
        return true;
    } catch (const cv::Exception &e) {
        if (errorMessage)
            *errorMessage = QStringLiteral("OpenCV 异常: %1").arg(QString::fromStdString(e.err));
        return false;
    } catch (const std::exception &e) {
        if (errorMessage)
            *errorMessage = QStringLiteral("执行异常: %1").arg(QString::fromLocal8Bit(e.what()));
        return false;
    } catch (...) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未知 OpenCV 执行异常");
        return false;
    }
}

} // namespace Selt

#endif // OPENCVGUARD_H
