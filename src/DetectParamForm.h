#pragma once

#include <functional>

#include <alg/GrainFilling.h>

class QObject;

namespace Ui {
class Widget;
}

//左下角检测参数和 DetectParam 的读写。字段表只维护一份
class DetectParamForm
{
public:
    //连通性、标签类型的下拉项只填一次
    static void initOptions(Ui::Widget *ui);

    //把参数写进控件。写的时候挡住信号，避免触发按钮刷新
    static void write(Ui::Widget *ui, const grain_filling::DetectParam &param);

    static grain_filling::DetectParam read(const Ui::Widget *ui);

    static bool thresholdChanged(const grain_filling::DetectParam &current, const grain_filling::DetectParam &applied);

    //面积、倍数、连通性、标签类型、补粒四舍五入。不含二值化阈值
    static bool otherChanged(const grain_filling::DetectParam &current, const grain_filling::DetectParam &applied);

    //任一参数变化都调用 onChanged
    static void bindChanges(Ui::Widget *ui, QObject *context, const std::function<void()> &onChanged);
};
