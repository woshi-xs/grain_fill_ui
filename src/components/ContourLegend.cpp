#include "ContourLegend.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

namespace {

//线表示链走向，实心圆表示链起点，空心框表示轮廓
QWidget *swatch(const QColor &color, bool filledPoint, bool line)
{
    auto *mark = new QFrame;
    mark->setFixedSize(18, 12);
    if (line)
    {
        mark->setStyleSheet(QStringLiteral("background: transparent; border-top: 2px solid %1;").arg(color.name()));
    }
    else if (filledPoint)
    {
        mark->setStyleSheet(QStringLiteral("background: %1; border-radius: 10px;").arg(color.name()));
    }
    else
    {
        mark->setStyleSheet(QStringLiteral("background: transparent; border: 2px solid %1;").arg(color.name()));
    }
    return mark;
}

void addItem(QHBoxLayout *layout, const QColor &color, const QString &text, bool filledPoint, bool line)
{
    layout->addWidget(swatch(color, filledPoint, line));
    layout->addWidget(new QLabel(text));
    layout->addSpacing(12);
}

}

ContourLegend::ContourLegend(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(28);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    addItem(layout, QColor(0, 255, 0), QStringLiteral("已入链"), false, false);
    addItem(layout, QColor(255, 0, 255), QStringLiteral("未入链"), false, false);
    addItem(layout, QColor(0, 0, 255), QStringLiteral("冲突"), false, false);
    addItem(layout, QColor(255, 0, 0), QStringLiteral("填补晶粒"), false, false);
    addItem(layout, QColor(255, 0, 0), QStringLiteral("链起点"), true, false);
    addItem(layout, QColor(255, 0, 0), QStringLiteral("走向"), false, true);
    addItem(layout, QColor(255, 255, 0), QStringLiteral("当前选中"), false, false);
    layout->addStretch(1);
}
