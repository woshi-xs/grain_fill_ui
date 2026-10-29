#pragma once

#include <vector>

#include <QGraphicsItem>

#include <alg/GrainFilling.h>

//整张图一个图元。轮廓点用来画，两张标签图用来点选
class GrainOverlayItem : public QGraphicsItem
{
public:
    GrainOverlayItem();

    void setDetection(const grain_filling::DetectResult *result, const cv::Mat *fillLabels, const std::vector<grain_filling::FillGrain> *fillGrains, const std::vector<std::vector<cv::Point>> *addedContours);

    enum class Pick
    {
        Miss,
        Movable, //绿色、蓝色、紫色或复制轮廓
        Fill
    };

    //Ctrl 同时增减晶粒和复制轮廓。填补轮廓只记下位置，不进入移除选中
    Pick selectAt(const QPointF &scenePos, bool ctrl);

    //拖动时只平移已选缓存，不重建全部轮廓
    void translateSelection(const QPointF &delta);

    //按当前轮廓点重做缓存
    void refresh();

    void clearSelection();
    void clearEdits();

    const std::vector<int> &selectedLabelIds() const { return m_selected; }
    const std::vector<int> &selectedAddedIndices() const { return m_selectedAdded; }
    bool currentFill(grain_filling::FillGrain &grain) const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    struct Shape
    {
        QPolygonF polygon;
        QRectF bounds;
        QColor color;
        int grainId = -1;
        int addedIndex = -1;
        int chainIndex = -1;
        int gapIndex = -1;
        int fillIndex = -1;
    };

    void rebuildCache();
    bool selected(int labelId) const;
    bool selectedAdded(int index) const;

    const grain_filling::DetectResult *m_result = nullptr;
    const cv::Mat *m_fillLabels = nullptr;
    const std::vector<grain_filling::FillGrain> *m_fillGrains = nullptr;

    const std::vector<std::vector<cv::Point>> *m_added = nullptr;

    std::vector<int> m_selected;
    std::vector<int> m_selectedAdded;
    bool m_hasFill = false;
    grain_filling::FillGrain m_fill;
    struct ChainLine
    {
        QPolygonF line;
        std::vector<int> labelIds;
    };

    struct StartDot
    {
        QPointF center;
        int labelId = -1;
    };

    std::vector<Shape> m_shapes;
    std::vector<ChainLine> m_chainLines;
    std::vector<StartDot> m_starts;
    QRectF m_bounds;
};
