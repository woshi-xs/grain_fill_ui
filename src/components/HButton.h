#ifndef HBUTTON_H
#define HBUTTON_H

#include <QAbstractButton>

class HButton : public QAbstractButton
{
    Q_OBJECT
public:
    explicit HButton(QWidget *parent = nullptr);

    /**
     * @brief 为双态图标设置普通态与选中态图片，合并至同一个QIcon
     * @param normalPath 普通状态图标文件路径
     * @param checkedPath 选中状态图标文件路径
     */
    void setIcons(const QString &normalPath, const QString &checkedPath);

    /**
     * @brief 设置按钮圆角半径
     * @param radius 圆角像素值
     */
    void setRoundRadius(unsigned int radius) { this->m_nRoundRadius = radius; update(); }

    /**
     * @brief 设置控件正常状态下的背景色
     * @param color 目标背景颜色
     */
    void setBgNormal(const QColor& color);

    /**
     * @brief 设置控件禁用状态下的背景色
     * @param color 目标背景颜色
     */
    void setBgDisabled(const QColor& color);

    /**
     * @brief 设置控件按下状态下的背景色
     * @param color 目标背景颜色
     */
    void setBgPressed(const QColor& color);

    /**
     * @brief 设置控件选中状态下的背景色
     * @param color 目标背景颜色
     */
    void setBgChecked(const QColor& color);

    /**
     * @brief 设置控件鼠标悬停状态下的背景色
     * @param color 目标背景颜色
     */
    void setBgHover(const QColor& color);
protected:
    void paintEvent(QPaintEvent *e) override;

private:
    unsigned int m_nRoundRadius = 4;    //按钮圆角

    QColor m_BgNormal = Qt::transparent;    // 正常状态背景（透明）
    QColor m_BgDisabled = QColor(0, 0, 0, 20);    // 禁用状态背景
    QColor m_BgPressed = QColor(213, 213, 213);     // 按下状态背景
    QColor m_BgChecked = Qt::transparent;     // 选中状态背景
    QColor m_BgHover = QColor(225, 225, 225);       // 鼠标悬停状态背景

    QIcon m_HoverIcon;  //悬浮时显示的svg

};

#endif // HBUTTON_H
