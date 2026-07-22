#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

struct RecipeVersion
{
    QString id;
    int number{1};
    QDateTime createdAt{QDateTime::currentDateTimeUtc()};
    QString author;
    QJsonObject definition;

    QJsonObject toJson() const;
    static RecipeVersion fromJson(const QJsonObject &obj);
};

struct RecipeMetadata
{
    QString id;
    QString name;
    QString description;
    QStringList tags;
    QDateTime createdAt{QDateTime::currentDateTimeUtc()};
    QDateTime updatedAt{QDateTime::currentDateTimeUtc()};
    QVector<RecipeVersion> versions;
    QString activeVersionId;

    QJsonObject toJson() const;
    static RecipeMetadata fromJson(const QJsonObject &obj);
    const RecipeVersion *activeVersion() const;
};

class RecipeStore
{
public:
    void clear();
    bool upsert(const RecipeMetadata &recipe, QString *error = nullptr);
    bool remove(const QString &recipeId);
    bool contains(const QString &recipeId) const;
    RecipeMetadata recipe(const QString &recipeId) const;
    QVector<RecipeMetadata> recipes() const;

    bool save(const QString &filePath, QString *error = nullptr) const;
    bool load(const QString &filePath, QString *error = nullptr);

private:
    QVector<RecipeMetadata> m_recipes;
};

} // namespace Selt
