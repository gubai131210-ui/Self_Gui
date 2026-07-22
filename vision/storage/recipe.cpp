#include "vision/storage/recipe.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace Selt {
namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QDateTime readDate(const QJsonObject &obj, const QString &key)
{
    return QDateTime::fromString(obj.value(key).toString(), Qt::ISODateWithMs);
}

} // namespace

QJsonObject RecipeVersion::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("number"), number},
        {QStringLiteral("createdAt"), createdAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("author"), author},
        {QStringLiteral("definition"), definition},
    };
}

RecipeVersion RecipeVersion::fromJson(const QJsonObject &obj)
{
    RecipeVersion version;
    version.id = obj.value(QStringLiteral("id")).toString();
    version.number = obj.value(QStringLiteral("number")).toInt(1);
    version.createdAt = readDate(obj, QStringLiteral("createdAt"));
    if (!version.createdAt.isValid())
        version.createdAt = QDateTime::currentDateTimeUtc();
    version.author = obj.value(QStringLiteral("author")).toString();
    version.definition = obj.value(QStringLiteral("definition")).toObject();
    return version;
}

QJsonObject RecipeMetadata::toJson() const
{
    QJsonArray versionArray;
    for (const RecipeVersion &version : versions)
        versionArray.append(version.toJson());
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("tags"), QJsonArray::fromStringList(tags)},
        {QStringLiteral("createdAt"), createdAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("updatedAt"), updatedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("activeVersionId"), activeVersionId},
        {QStringLiteral("versions"), versionArray},
    };
}

RecipeMetadata RecipeMetadata::fromJson(const QJsonObject &obj)
{
    RecipeMetadata recipe;
    recipe.id = obj.value(QStringLiteral("id")).toString();
    recipe.name = obj.value(QStringLiteral("name")).toString();
    recipe.description = obj.value(QStringLiteral("description")).toString();
    for (const QJsonValue &tag : obj.value(QStringLiteral("tags")).toArray())
        recipe.tags.append(tag.toString());
    recipe.createdAt = readDate(obj, QStringLiteral("createdAt"));
    recipe.updatedAt = readDate(obj, QStringLiteral("updatedAt"));
    if (!recipe.createdAt.isValid())
        recipe.createdAt = QDateTime::currentDateTimeUtc();
    if (!recipe.updatedAt.isValid())
        recipe.updatedAt = recipe.createdAt;
    recipe.activeVersionId = obj.value(QStringLiteral("activeVersionId")).toString();
    for (const QJsonValue &value : obj.value(QStringLiteral("versions")).toArray())
        recipe.versions.append(RecipeVersion::fromJson(value.toObject()));
    return recipe;
}

const RecipeVersion *RecipeMetadata::activeVersion() const
{
    for (const RecipeVersion &version : versions) {
        if (version.id == activeVersionId)
            return &version;
    }
    return nullptr;
}

void RecipeStore::clear()
{
    m_recipes.clear();
}

bool RecipeStore::upsert(const RecipeMetadata &recipeValue, QString *error)
{
    if (recipeValue.id.trimmed().isEmpty()) {
        setError(error, QStringLiteral("配方 ID 不能为空"));
        return false;
    }
    for (RecipeMetadata &recipe : m_recipes) {
        if (recipe.id == recipeValue.id) {
            recipe = recipeValue;
            return true;
        }
    }
    m_recipes.append(recipeValue);
    return true;
}

bool RecipeStore::remove(const QString &recipeId)
{
    for (int i = 0; i < m_recipes.size(); ++i) {
        if (m_recipes.at(i).id == recipeId) {
            m_recipes.removeAt(i);
            return true;
        }
    }
    return false;
}

bool RecipeStore::contains(const QString &recipeId) const
{
    return !recipe(recipeId).id.isEmpty();
}

RecipeMetadata RecipeStore::recipe(const QString &recipeId) const
{
    for (const RecipeMetadata &recipe : m_recipes) {
        if (recipe.id == recipeId)
            return recipe;
    }
    return {};
}

QVector<RecipeMetadata> RecipeStore::recipes() const
{
    return m_recipes;
}

bool RecipeStore::save(const QString &filePath, QString *error) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonArray array;
    for (const RecipeMetadata &recipeValue : m_recipes)
        array.append(recipeValue.toJson());
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("recipes"), array}})
                   .toJson(QJsonDocument::Indented));
    return true;
}

bool RecipeStore::load(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, parseError.errorString());
        return false;
    }
    QVector<RecipeMetadata> loaded;
    for (const QJsonValue &value : document.object().value(QStringLiteral("recipes")).toArray())
        loaded.append(RecipeMetadata::fromJson(value.toObject()));
    m_recipes = std::move(loaded);
    return true;
}

} // namespace Selt
