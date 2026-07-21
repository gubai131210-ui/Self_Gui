#ifndef DOCUMENTSERIALIZER_H
#define DOCUMENTSERIALIZER_H

#include "core/model/document.h"

#include <QString>

class DocumentSerializer
{
public:
    static constexpr int CurrentVersion = 3;
    static constexpr const char *FormatName = "selt-document";

    static bool saveToFile(const Document &document, const QString &filePath, QString *errorMessage = nullptr);
    static bool loadFromFile(Document &document, const QString &filePath, QString *errorMessage = nullptr);

    static QByteArray toJson(const Document &document);
    static bool fromJson(Document &document, const QByteArray &data, QString *errorMessage = nullptr);
};

#endif // DOCUMENTSERIALIZER_H
