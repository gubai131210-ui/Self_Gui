#ifndef NODESTYLE_H
#define NODESTYLE_H

#include <QColor>
#include <QJsonObject>
#include <QString>

struct NodeStyle
{
    QColor fillColor{QColor(45, 48, 54)};
    QColor borderColor{QColor(255, 140, 0)};
    qreal borderWidth{1.5};
    QColor textColor{QColor(220, 224, 230)};
    int fontSize{11};
    qreal cornerRadius{6.0};

    QJsonObject toJson() const;
    static NodeStyle fromJson(const QJsonObject &obj);
};

#endif // NODESTYLE_H
