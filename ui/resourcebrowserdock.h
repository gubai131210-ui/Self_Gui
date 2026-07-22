#ifndef RESOURCEBROWSERDOCK_H
#define RESOURCEBROWSERDOCK_H

#include <QWidget>

#if SELT_HAS_OPENCV
#include "vision/storage/projectresourcestore.h"
#endif

class QListWidget;
class QLabel;
class QComboBox;
class QPushButton;

class ResourceBrowserDock : public QWidget
{
    Q_OBJECT
public:
    explicit ResourceBrowserDock(QWidget *parent = nullptr);

#if SELT_HAS_OPENCV
    void setStore(const Selt::ProjectResourceStore *store);
#endif
    void reload();

signals:
    void resourceActivated(const QString &resourceId);

private:
    void onFilterChanged();
    void onItemActivated();

    QComboBox *m_kindFilter{nullptr};
    QListWidget *m_list{nullptr};
    QLabel *m_summary{nullptr};
    QPushButton *m_refreshBtn{nullptr};
#if SELT_HAS_OPENCV
    const Selt::ProjectResourceStore *m_store{nullptr};
#endif
};

#endif // RESOURCEBROWSERDOCK_H
