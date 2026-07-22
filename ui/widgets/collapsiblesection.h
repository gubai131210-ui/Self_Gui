#ifndef COLLAPSIBLESECTION_H
#define COLLAPSIBLESECTION_H

#include <QWidget>

class QFrame;
class QLabel;
class QVBoxLayout;

/// Blender/Unity 风格可折叠分区：点击标题折叠，可选 QSettings 持久化。
class CollapsibleSection : public QWidget
{
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString &title,
                                const QString &settingsKey = QString(),
                                bool defaultExpanded = true,
                                QWidget *parent = nullptr);

    void setBodyWidget(QWidget *body);
    QVBoxLayout *contentLayout() const { return m_bodyLayout; }
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    QString title() const;
    void setTitle(const QString &title);

signals:
    void toggled(bool expanded);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyExpandedState();
    void persistExpanded() const;
    bool loadExpandedDefault(bool fallback) const;

    QFrame *m_header{nullptr};
    QLabel *m_arrow{nullptr};
    QLabel *m_titleLabel{nullptr};
    QWidget *m_bodyHost{nullptr};
    QVBoxLayout *m_bodyLayout{nullptr};
    QWidget *m_body{nullptr};
    QString m_settingsKey;
    bool m_expanded{true};
};

#endif // COLLAPSIBLESECTION_H
