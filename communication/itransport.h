#pragma once

#include "communication/types.h"

#include <QByteArray>
#include <functional>
#include <memory>

namespace Selt {
namespace Communication {

class ITransport
{
public:
    virtual ~ITransport() = default;
    virtual QString name() const = 0;
    virtual CommunicationDiagnostic open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual CommunicationDiagnostic writeBytes(const QByteArray &data) = 0;
    /// Read until minBytes collected or timeout. maxBytes caps the buffer.
    virtual CommunicationDiagnostic readBytes(int minBytes, int maxBytes, int timeoutMs,
                                              QByteArray *out) = 0;
};

using TransportFactory = std::function<std::unique_ptr<ITransport>()>;

} // namespace Communication
} // namespace Selt
