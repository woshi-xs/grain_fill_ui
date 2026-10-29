#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <functional>
#include <vector>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QWheelEvent>
#include <QMouseEvent>

#include <alg/GrainFilling.h>

class GrainOverlayItem;

class imageview : public QGraphicsView
{
    Q_OBJECT
public:
    explicit imageview(QWidget *parent = nullptr);

    //设置图像。第一次显示时自适应窗口
    void setImage(const QImage &img);

    //只更换底图，不改变缩放
    void setBackground(const QImage &img);

    //清除图像
    void clearImage();

    //图片自适应组件
    void fitImageToView();

    //设置最小和最大的缩放倍数限制
    void setZoomRange(double min, double max);

    //选中变化后直接调用，不用信号
    void setEditCallback(std::function<void()> callback);

    //拖动轮廓时按场景坐标位移调用。松手时调用结束回调
    void setDragCallback(std::function<void(QPointF)> callback);
    void setDragEndCallback(std::function<void()> callback);

    void translateSelection(const QPointF &delta);
    void rebuildOverlay();

    void setDetection(const grain_filling::DetectResult *result, const cv::Mat *fillLabels, const std::vector<grain_filling::FillGrain> *fillGrains, const std::vector<std::vector<cv::Point>> *addedContours);
    void clearDetection();
    void clearSelection();
    void clearEdits();

    std::vector<int> selectedLabelIds() const;
    std::vector<int> selectedAddedIndices() const;
    bool currentFill(grain_filling::FillGrain &grain) const;

protected:

    void wheelEvent(QWheelEvent *event) override;      //以鼠标所在位置为中心缩放
    void mousePressEvent(QMouseEvent *event) override; //中键平移，左键交给视图拖拽
    void mouseMoveEvent(QMouseEvent *event) override;  //中键按住时平移
    void mouseReleaseEvent(QMouseEvent *event) override;

private:

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_image;
    GrainOverlayItem *m_overlay = nullptr;
    std::function<void()> m_editCallback;
    std::function<void(QPointF)> m_dragCallback;
    std::function<void()> m_dragEndCallback;
    bool m_panning = false;                //中键平移中
    bool m_dragging = false;              //左键拖动轮廓
    QPoint m_lastPan;                      //上一帧中键位置，视口坐标
    QPoint m_lastDrag;                     //上一帧拖动位置，视口坐标

    double m_zoomFactor = 1.0;  // 当前缩放系数
    double m_minZoom = 0.01;     // 最小为自适应比例的 0.01 倍
    double m_maxZoom = 50.0;    // 最大为自适应比例的 50 倍
};

#endif // IMAGEVIEW_H
