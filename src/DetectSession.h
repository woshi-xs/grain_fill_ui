#pragma once

#include <filesystem>
#include <string>

#include <alg/GrainFilling.h>
#include <alg/ImageUtil.h>

//一次检测用到的图、参数和结果。界面只负责选文件和刷新按钮
class DetectSession
{
public:
    image::ImageSession image;
    grain_filling::DetectParam param{};
    grain_filling::DetectParam applied{}; //上次成功检测，或成功生成二值图时用过的参数
    grain_filling::DetectResult result{};
    cv::Mat fillLabels; //填补轮廓标签图，像素值等于 fillGrains 下标
    std::vector<grain_filling::FillGrain> fillGrains;
    std::vector<std::vector<cv::Point>> addedContours; //从绿色晶粒复制出来的轮廓
    bool grainEdited = false;         //移除或复制过晶粒，需要按轮廓重做 mask
    bool binaryPendingDetect = false; //二值图已按新阈值生成，重新执行算法还没使用它

    //path 用 UTF-8。读图失败时 src 为空，返回 false
    bool loadImage(const std::filesystem::path &path, const std::string &utf8Path);

    int runDetect();   //清空结果后 detect
    int rerunDetect(); //先克隆二值图，再 detectMask
    int rebuildBinary();

    //画一张结果图，给保存用
    void bakeOverlay();

    void acceptCurrentParam(); //整组参数都已经跑过
    void acceptNewBinary();    //只更新阈值，并要求重新执行算法

    //按当前链重做填补标签图。链或图像尺寸无效时清空两份输出
    void rebuildFillLabels();

    //把鼠标位移收成整数步，不足 1 像素的留到下一次
    cv::Point dragStep(double dx, double dy);
    void endDrag();
    void moveGrain(int labelId, int dx, int dy);
    void moveAdded(int index, int dx, int dy);

private:
    double m_dragRemainX = 0;
    double m_dragRemainY = 0;
};
