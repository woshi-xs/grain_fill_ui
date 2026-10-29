#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

#include <components/HWindow.h>

#include "DetectSession.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public HWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

    //捕获窗框放大/还原事件，由事件控制按钮图标。防止双击标题栏按钮样式不改变
    void changeEvent(QEvent *event) override;

private slots:
    void on_min_clicked();

    void on_max_clicked();

    void on_close_clicked();

    void on_loadAndDetectButton_clicked(); //选择图片，预处理并检测，首次自适应显示

    void on_rebuildBinaryButton_clicked(); //按当前阈值重生成二值图。成功后才允许重新执行算法

    void on_rerunDetectButton_clicked(); //用当前二值图和参数重新detectMask，不改变缩放

    void on_resetParamButton_clicked(); //把算法参数恢复为默认值

    void on_removeSelectedButton_clicked();

    void on_copyContourButton_clicked();

    void on_insertGrainsButton_clicked();

    void on_applyGapButton_clicked();

    void on_backgroundCombo_currentIndexChanged(int index); //切换原图或二值图

private:
    Ui::Widget *ui;

    void initTopTitleBarButton();

    void initLeftTitleBarButton();

    //把算法参数和左下角控件恢复为 DetectParam 的默认值
    void resetDetectParam();

    //阈值改过则只能先生成二值图。其余参数改过，或二值图已更新还没重新检测时，才允许重新执行算法
    void syncParamButtons();

    //检测成功后的共同收尾。refitView 为 true 时回到原图并自适应
    void finishDetect(bool refitView, const QString &status);

    void reportFailure(const QString &status, const char *log);

    //按 m_backgroundMode 换底图。单通道二值图先转成 BGR
    void applyBackground();

    //按当前选中刷新移除、复制、插入、应用
    void updateContourActions();

    //把链和填补标签图重新交给视图
    void publishContours();

    bool isGreen(int labelId) const;

    //正好两颗绿色，且在同一条链上相邻。labelA 到 labelB 按链方向
    bool selectedAdjacentGreens(int &chainIndex, int &labelA, int &labelB) const;

    QString meanSizeText() const;

    //链下标从 0 开始。从左向右时链下标是行、链内下标是列；从上到下时相反
    QString chainPlaceText(int labelId) const;

    QString selectionStatusText() const;

    DetectSession m_session;

    enum class BackgroundMode
    {
        Source, //原图
        Binary  //二值图
    };

    BackgroundMode m_backgroundMode = BackgroundMode::Source;
};
#endif // WIDGET_H
