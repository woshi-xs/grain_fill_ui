#pragma once

#include <QWidget>

/**
 * @brief 轮廓颜色图例
 * @note Qt的QColor按RGB，绿色(0,255,0)，紫色(255,0,255)，蓝色(0,0,255)，红色(255,0,0)，选中为黄色(255,255,0)。
 */
class ContourLegend : public QWidget
{
    Q_OBJECT
public:
    explicit ContourLegend(QWidget *parent = nullptr);
};
