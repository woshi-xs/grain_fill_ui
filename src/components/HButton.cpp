#include "hbutton.h"
#include <QPainter>
#include <QPainterPath>

HButton::HButton(QWidget *parent)
    : QAbstractButton(parent)
{
    //如果设置为false则状态默认为QIcon::off
    this->setCheckable(true);
    this->setIconSize(QSize(24, 24));
    this->setMinimumSize(40,40);
    // 开启鼠标悬停检测，否则 underMouse() 不准
    this->setAttribute(Qt::WA_Hover, true);
}


// 把两张 SVG 塞进同一个 QIcon
void HButton::setIcons(const QString &normalPath, const QString &checkedPath)
{
    QIcon dualIcon;
    // 贴上 QIcon::Off 标签（没选中时用这张）
    dualIcon.addFile(normalPath, QSize(), QIcon::Normal, QIcon::Off);

    // 贴上 QIcon::On 标签（选中时用这张）
    dualIcon.addFile(checkedPath, QSize(), QIcon::Normal, QIcon::On);

    // 交给父类保管
    this->setIcon(dualIcon);
    update();
}

void HButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 画背景
    QColor bgColor = m_BgNormal;//正常
    if (!isEnabled()) {
        bgColor = m_BgDisabled;// 禁用
    } else if (isDown()) {
        bgColor = m_BgPressed;// 按下
    } else if (underMouse()) {
        bgColor = m_BgHover;// 仅悬停
    } else if (isChecked()) {
        bgColor = m_BgChecked;// 选中
    }

    // 设置圆角裁剪区域
    QPainterPath path;
    path.addRoundedRect(rect(), m_nRoundRadius, m_nRoundRadius);
    p.setClipPath(path);

    // 绘制背景
    p.setBrush(bgColor);
    p.setPen(Qt::NoPen);
    p.drawRect(rect());

    // 图标绘制
    if (!icon().isNull()) {
        QRect iconRect(QPoint(), iconSize());
        iconRect.moveCenter(rect().center());

        // 告诉 QIcon：当前按钮是可用还是禁用
        QIcon::Mode mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;

        // 告诉 QIcon：当前按钮是选中 (On) 还是没选中 (Off)
        QIcon::State state = isChecked() ? QIcon::On : QIcon::Off;

        // QIcon 会根据传的 state 动选择 On / Off SVG
        icon().paint(&p, iconRect, Qt::AlignCenter, mode, state);
    }
}

void HButton::setBgNormal(const QColor& color)
{
    m_BgNormal = color;
    update();
}

void HButton::setBgDisabled(const QColor& color)
{
    m_BgDisabled = color;
    update();
}

void HButton::setBgPressed(const QColor& color)
{
    m_BgPressed = color;
    update();
}

void HButton::setBgChecked(const QColor& color)
{
    m_BgChecked = color;
    update();
}

void HButton::setBgHover(const QColor& color)
{
    m_BgHover = color;
    update();
}

