#include "GrainOverlayItem.h"

#include <algorithm>

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace {

QPolygonF toPolygon(const std::vector<cv::Point> &contour)
{
    QPolygonF polygon;
    polygon.reserve(static_cast<int>(contour.size()));
    for (const cv::Point &point : contour)
        polygon << QPointF(point.x, point.y);
    return polygon;
}

QPen cosmeticPen(const QColor &color, int width)
{
    QPen pen(color);
    pen.setCosmetic(true);
    pen.setWidth(width);
    return pen;
}

}

GrainOverlayItem::GrainOverlayItem()
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setZValue(1);
}

void GrainOverlayItem::setDetection(const grain_filling::DetectResult *result, const cv::Mat *fillLabels, const std::vector<grain_filling::FillGrain> *fillGrains, const std::vector<std::vector<cv::Point>> *addedContours)
{
    m_result = result;
    m_fillLabels = fillLabels;
    m_fillGrains = fillGrains;
    m_added = addedContours;
    rebuildCache();
}

GrainOverlayItem::Pick GrainOverlayItem::selectAt(const QPointF &scenePos, bool ctrl)
{
    const cv::Point2f point(static_cast<float>(scenePos.x()), static_cast<float>(scenePos.y()));
    if (m_added != nullptr)
    {
        for (int i = static_cast<int>(m_added->size()) - 1; i >= 0; --i)
        {
            const std::vector<cv::Point> &contour = (*m_added)[static_cast<size_t>(i)];
            if (contour.size() < 3 || cv::pointPolygonTest(contour, point, false) < 0)
                continue;
            m_hasFill = false;
            if (ctrl)
            {
                const auto found = std::find(m_selectedAdded.begin(), m_selectedAdded.end(), i);
                if (found == m_selectedAdded.end())
                    m_selectedAdded.push_back(i);
                else
                    m_selectedAdded.erase(found);
            }
            else
            {
                m_selected.clear();
                m_selectedAdded.clear();
                m_selectedAdded.push_back(i);
            }
            update();
            return Pick::Movable;
        }
    }

    const cv::Point pixel(cvRound(scenePos.x()), cvRound(scenePos.y()));
    int labelId = 0;
    if (m_fillLabels != nullptr && m_fillGrains != nullptr && grain_filling::labelIdAt(*m_fillLabels, pixel, labelId) == 0 && labelId > 0 && labelId < static_cast<int>(m_fillGrains->size()))
    {
        m_hasFill = true;
        m_fill = (*m_fillGrains)[static_cast<size_t>(labelId)];
        m_selected.clear();
        m_selectedAdded.clear();
        update();
        return Pick::Fill;
    }

    if (m_result != nullptr)
    {
        const std::vector<grain_filling::GrainInfo> &grains = m_result->grainList;
        for (int i = static_cast<int>(grains.size()) - 1; i >= 1; --i)
        {
            const grain_filling::GrainInfo &grain = grains[static_cast<size_t>(i)];
            if (!grain.enable || grain.contour.size() < 3 || cv::pointPolygonTest(grain.contour, point, false) < 0)
                continue;
            m_hasFill = false;
            if (ctrl)
            {
                const auto found = std::find(m_selected.begin(), m_selected.end(), i);
                if (found == m_selected.end())
                    m_selected.push_back(i);
                else
                    m_selected.erase(found);
            }
            else
            {
                m_selectedAdded.clear();
                m_selected.clear();
                m_selected.push_back(i);
            }
            update();
            return Pick::Movable;
        }
    }

    m_selected.clear();
    m_selectedAdded.clear();
    m_hasFill = false;
    update();
    return Pick::Miss;
}

void GrainOverlayItem::refresh()
{
    rebuildCache();
}

void GrainOverlayItem::translateSelection(const QPointF &delta)
{
    for (Shape &shape : m_shapes)
    {
        const bool grain = shape.grainId >= 0 && selected(shape.grainId);
        const bool added = shape.addedIndex >= 0 && selectedAdded(shape.addedIndex);
        if (!grain && !added)
            continue;
        shape.polygon.translate(delta);
        shape.bounds.translate(delta);
    }
    for (ChainLine &line : m_chainLines)
    {
        for (int i = 0; i < static_cast<int>(line.labelIds.size()); ++i)
        {
            if (!selected(line.labelIds[static_cast<size_t>(i)]))
                continue;
            line.line[i] += delta;
        }
    }
    for (StartDot &dot : m_starts)
    {
        if (selected(dot.labelId))
            dot.center += delta;
    }
    update();
}

void GrainOverlayItem::clearSelection()
{
    m_selected.clear();
    m_selectedAdded.clear();
    m_hasFill = false;
    update();
}

void GrainOverlayItem::clearEdits()
{
    clearSelection();
}

bool GrainOverlayItem::currentFill(grain_filling::FillGrain &grain) const
{
    if (!m_hasFill)
        return false;
    grain = m_fill;
    return true;
}

QRectF GrainOverlayItem::boundingRect() const
{
    return m_bounds;
}

void GrainOverlayItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setBrush(Qt::NoBrush);
    const QRectF exposed = option->exposedRect;
    const QPen highlight = cosmeticPen(QColor(255, 255, 0), 2);
    for (const Shape &shape : m_shapes)
    {
        if (!shape.bounds.intersects(exposed))
            continue;
        painter->setPen(cosmeticPen(shape.color, 1));
        painter->drawPolygon(shape.polygon);
        const bool fillHit = m_hasFill && shape.grainId < 0 && shape.addedIndex < 0 && shape.chainIndex == m_fill.chainIndex && shape.gapIndex == m_fill.gapIndex && shape.fillIndex == m_fill.fillIndex;
        if ((shape.grainId >= 0 && selected(shape.grainId)) || (shape.addedIndex >= 0 && selectedAdded(shape.addedIndex)) || fillHit)
        {
            painter->setPen(highlight);
            painter->drawPolygon(shape.polygon);
        }
    }

    painter->setPen(cosmeticPen(QColor(255, 0, 0), 1));
    for (const ChainLine &line : m_chainLines)
        painter->drawPolyline(line.line);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 0, 0));
    for (const StartDot &dot : m_starts)
        painter->drawEllipse(dot.center, 2, 2);
}

void GrainOverlayItem::rebuildCache()
{
    prepareGeometryChange();
    m_shapes.clear();
    m_chainLines.clear();
    m_starts.clear();
    m_bounds = QRectF();
    if (m_result == nullptr || m_result->labels.empty())
    {
        update();
        return;
    }

    m_bounds = QRectF(0, 0, m_result->labels.cols, m_result->labels.rows);
    const std::vector<int> &conflicts = m_result->conflictLabelIds;
    const auto conflicted = [&conflicts](int labelId) {
        return std::find(conflicts.begin(), conflicts.end(), labelId) != conflicts.end();
    };

    for (int i = 1; i < static_cast<int>(m_result->grainList.size()); ++i)
    {
        const grain_filling::GrainInfo &grain = m_result->grainList[static_cast<size_t>(i)];
        if (!grain.enable || grain.contour.empty())
            continue;
        Shape shape;
        shape.polygon = toPolygon(grain.contour);
        shape.bounds = shape.polygon.boundingRect();
        shape.grainId = i;
        if (conflicted(i))
            shape.color = QColor(0, 0, 255);
        else if (grain.inChain)
            shape.color = QColor(0, 255, 0);
        else
            shape.color = QColor(255, 0, 255);
        m_shapes.push_back(std::move(shape));
    }

    for (int chainIndex = 0; chainIndex < static_cast<int>(m_result->chains.size()); ++chainIndex)
    {
        const grain_filling::GrainRayChain &chain = m_result->chains[static_cast<size_t>(chainIndex)];
        for (int gapIndex = 0; gapIndex < static_cast<int>(chain.gaps.size()); ++gapIndex)
        {
            const grain_filling::RayGap &gap = chain.gaps[static_cast<size_t>(gapIndex)];
            for (int fillIndex = 0; fillIndex < static_cast<int>(gap.fillContours.size()); ++fillIndex)
            {
                const std::vector<cv::Point> &contour = gap.fillContours[static_cast<size_t>(fillIndex)];
                if (contour.empty())
                    continue;
                Shape shape;
                shape.polygon = toPolygon(contour);
                shape.bounds = shape.polygon.boundingRect();
                shape.color = QColor(255, 0, 0);
                shape.chainIndex = chainIndex;
                shape.gapIndex = gapIndex;
                shape.fillIndex = fillIndex;
                m_shapes.push_back(std::move(shape));
            }
        }
    }

    if (m_added != nullptr)
    {
        for (int i = 0; i < static_cast<int>(m_added->size()); ++i)
        {
            const std::vector<cv::Point> &contour = (*m_added)[static_cast<size_t>(i)];
            if (contour.empty())
                continue;
            Shape shape;
            shape.polygon = toPolygon(contour);
            shape.bounds = shape.polygon.boundingRect();
            shape.color = QColor(0, 255, 0);
            shape.addedIndex = i;
            m_shapes.push_back(std::move(shape));
        }
    }

    const std::vector<grain_filling::GrainInfo> &grains = m_result->grainList;
    for (const grain_filling::GrainRayChain &chain : m_result->chains)
    {
        ChainLine line;
        for (int labelId : chain.labelIds)
        {
            if (labelId <= 0 || labelId >= static_cast<int>(grains.size()))
                continue;
            const cv::Point2d &centroid = grains[static_cast<size_t>(labelId)].centroid;
            line.line << QPointF(centroid.x, centroid.y);
            line.labelIds.push_back(labelId);
        }
        if (line.line.size() >= 2)
            m_chainLines.push_back(std::move(line));
    }

    for (int labelId : m_result->lineStartIndices)
    {
        if (labelId <= 0 || labelId >= static_cast<int>(grains.size()))
            continue;
        const cv::Point2d &centroid = grains[static_cast<size_t>(labelId)].centroid;
        m_starts.push_back(StartDot{QPointF(centroid.x, centroid.y), labelId});
    }
    update();
}

bool GrainOverlayItem::selected(int labelId) const
{
    return std::find(m_selected.begin(), m_selected.end(), labelId) != m_selected.end();
}

bool GrainOverlayItem::selectedAdded(int index) const
{
    return std::find(m_selectedAdded.begin(), m_selectedAdded.end(), index) != m_selectedAdded.end();
}
