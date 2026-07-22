#include "ui/theme/iconprovider.h"



#include <QFile>

#include <QPainter>

#include <QPixmap>

#include <QSvgRenderer>



namespace Selt {



namespace {



QString pathFor(IconId id)

{

    switch (id) {

    case IconId::New: return QStringLiteral(":/selt/icons/new.svg");

    case IconId::Open: return QStringLiteral(":/selt/icons/open.svg");

    case IconId::Save: return QStringLiteral(":/selt/icons/save.svg");

    case IconId::SaveAs: return QStringLiteral(":/selt/icons/save_as.svg");

    case IconId::Undo: return QStringLiteral(":/selt/icons/undo.svg");

    case IconId::Redo: return QStringLiteral(":/selt/icons/redo.svg");

    case IconId::Copy: return QStringLiteral(":/selt/icons/copy.svg");

    case IconId::Paste: return QStringLiteral(":/selt/icons/paste.svg");

    case IconId::Delete: return QStringLiteral(":/selt/icons/delete.svg");

    case IconId::ZoomIn: return QStringLiteral(":/selt/icons/zoom_in.svg");

    case IconId::ZoomOut: return QStringLiteral(":/selt/icons/zoom_out.svg");

    case IconId::Fit: return QStringLiteral(":/selt/icons/fit.svg");

    case IconId::Grid: return QStringLiteral(":/selt/icons/grid.svg");

    case IconId::Snap: return QStringLiteral(":/selt/icons/snap.svg");

    case IconId::Run: return QStringLiteral(":/selt/icons/run.svg");

    case IconId::Loop: return QStringLiteral(":/selt/icons/loop.svg");

    case IconId::Stop: return QStringLiteral(":/selt/icons/stop.svg");

    case IconId::ClearResult: return QStringLiteral(":/selt/icons/clear.svg");

    case IconId::AddFlow: return QStringLiteral(":/selt/icons/add_flow.svg");

    case IconId::Rename: return QStringLiteral(":/selt/icons/rename.svg");

    case IconId::RemoveFlow: return QStringLiteral(":/selt/icons/remove_flow.svg");

    case IconId::Nodes: return QStringLiteral(":/selt/icons/nodes.svg");

    case IconId::Properties: return QStringLiteral(":/selt/icons/properties.svg");

    case IconId::Results: return QStringLiteral(":/selt/icons/results.svg");

    case IconId::Settings: return QStringLiteral(":/selt/icons/settings.svg");

    case IconId::Theme: return QStringLiteral(":/selt/icons/theme.svg");

    case IconId::LayoutReset: return QStringLiteral(":/selt/icons/layout_reset.svg");

    case IconId::StatusIdle: return QStringLiteral(":/selt/icons/status_idle.svg");

    case IconId::StatusRun: return QStringLiteral(":/selt/icons/status_run.svg");

    case IconId::StatusOk: return QStringLiteral(":/selt/icons/status_ok.svg");

    case IconId::StatusFail: return QStringLiteral(":/selt/icons/status_fail.svg");

    case IconId::Node: return QStringLiteral(":/selt/icons/node.svg");

    }

    return {};

}



QIcon loadSvgIcon(const QString &path, int size)

{

    QIcon result;

    if (path.isEmpty() || !QFile::exists(path))

        return result;



    QSvgRenderer renderer(path);

    if (!renderer.isValid())

        return result;



    QPixmap pixmap(size, size);

    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    painter.setRenderHint(QPainter::Antialiasing, true);

    renderer.render(&painter);

    painter.end();

    result.addPixmap(pixmap);

    return result;

}



QString visionResourceStem(const QString &iconKey)

{

    if (iconKey.isEmpty())

        return {};



    // typeId: vision.imageLoader / communication.tcp.request -> underscore stem

    if (iconKey.startsWith(QStringLiteral("vision."))

        || iconKey.startsWith(QStringLiteral("communication.")))

        return QString(iconKey).replace(QLatin1Char('.'), QLatin1Char('_'));



    // already a stem: vision_imageLoader / cat_input

    if (iconKey.startsWith(QStringLiteral("vision_"))

        || iconKey.startsWith(QStringLiteral("communication_"))

        || iconKey.startsWith(QStringLiteral("cat_")))

        return iconKey;



    // Chinese / English category fallbacks

    if (iconKey == QStringLiteral("输入") || iconKey == QStringLiteral("input"))

        return QStringLiteral("cat_input");

    if (iconKey == QStringLiteral("预处理") || iconKey == QStringLiteral("process"))

        return QStringLiteral("cat_preprocess");

    if (iconKey == QStringLiteral("区域") || iconKey == QStringLiteral("region"))

        return QStringLiteral("cat_region");

    if (iconKey == QStringLiteral("定位") || iconKey == QStringLiteral("locate"))

        return QStringLiteral("cat_locate");

    if (iconKey == QStringLiteral("测量") || iconKey == QStringLiteral("measure"))

        return QStringLiteral("cat_measure");

    if (iconKey == QStringLiteral("判定") || iconKey == QStringLiteral("decision"))

        return QStringLiteral("cat_decision");

    if (iconKey == QStringLiteral("逻辑") || iconKey == QStringLiteral("logic"))

        return QStringLiteral("cat_logic");

    if (iconKey == QStringLiteral("通讯") || iconKey == QStringLiteral("communication"))

        return QStringLiteral("cat_output");

    if (iconKey == QStringLiteral("数据") || iconKey == QStringLiteral("data"))

        return QStringLiteral("cat_logic");

    if (iconKey == QStringLiteral("输出") || iconKey == QStringLiteral("output"))

        return QStringLiteral("cat_output");

    if (iconKey == QStringLiteral("模板") || iconKey == QStringLiteral("template"))

        return QStringLiteral("cat_template");

    if (iconKey == QStringLiteral("插件") || iconKey == QStringLiteral("plugin"))

        return QStringLiteral("cat_plugin");



    return {};

}



QHash<QString, QIcon> &visionCache()

{

    static QHash<QString, QIcon> instance;

    return instance;

}



} // namespace



QHash<int, QIcon> &IconProvider::cache()

{

    static QHash<int, QIcon> instance;

    return instance;

}



QString IconProvider::resourcePath(IconId id)

{

    return pathFor(id);

}



void IconProvider::clearCache()

{

    cache().clear();

    visionCache().clear();

}



QIcon IconProvider::icon(IconId id, int size)

{

    const int key = (static_cast<int>(id) << 16) | (size & 0xffff);

    if (cache().contains(key))

        return cache().value(key);



    QIcon result = loadSvgIcon(pathFor(id), size);

    cache().insert(key, result);

    return result;

}



QIcon IconProvider::visionIcon(const QString &iconKey, int size)

{

    const QString cacheKey = iconKey + QLatin1Char('@') + QString::number(size);

    if (visionCache().contains(cacheKey))

        return visionCache().value(cacheKey);



    QIcon result;

    const QString stem = visionResourceStem(iconKey);

    if (!stem.isEmpty()) {

        const QString path = QStringLiteral(":/selt/icons/vision/%1.svg").arg(stem);

        result = loadSvgIcon(path, size);

    }



    if (result.isNull())

        result = icon(IconId::Node, size);



    visionCache().insert(cacheKey, result);

    return result;

}



} // namespace Selt


