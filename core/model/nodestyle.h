#ifndef NODESTYLE_H
#define NODESTYLE_H

#include <QColor>
#include <QJsonObject>
#include <QString>

struct NodeStyle
{
    QColor fillColor{QColor(230, 240, 255)};
    QColor borderColor{QColor(60, 100, 180)};
    qreal borderWidth{2.0};
    QColor textColor{QColor(20, 20, 20)};
    int fontSize{12};
    qreal cornerRadius{8.0};

    QJsonObject toJson() const;
    static NodeStyle fromJson(const QJsonObject &obj);
};

#endif // NODESTYLE_H
