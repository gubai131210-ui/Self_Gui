#ifndef DOCUMENTSETTINGS_H
#define DOCUMENTSETTINGS_H

#include <QColor>
#include <QJsonObject>

struct DocumentSettings
{
    int gridSize{20};
    bool snapToGrid{true};
    bool showGrid{true};
    QColor backgroundColor{QColor(245, 245, 248)};

    QJsonObject toJson() const;
    static DocumentSettings fromJson(const QJsonObject &obj);
};

#endif // DOCUMENTSETTINGS_H
