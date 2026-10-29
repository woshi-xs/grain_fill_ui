#include "ImageView.h"

#include "GrainOverlayItem.h"

#include <QScrollBar>

#include <algorithm>

imageview::imageview(QWidget *parent)
    : QGraphicsView(parent),
    m_scene(new QGraphicsScene(this)),
    m_image(nullptr)
{
    setScene(m_scene);
    m_overlay = new GrainOverlayItem;
    m_scene->addItem(m_overlay);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    //关闭滚动条。滚轮缩放时若滚动条显隐变化，会触发布局变化，已放大的视图又会被适配回去
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //以鼠标所在位置为缩放锚点
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    //左键不拖动图片。平移只在中键按下时处理
    setDragMode(QGraphicsView::NoDrag);
}
// 设置图像
void imageview::setImage(const QImage &img)
{
    if (img.isNull()) {
        clearImage();
        return;
    }

    if (!m_image)
    {
        m_image = m_scene->addPixmap(QPixmap::fromImage(img));
    }
    else
    {
        m_image->setPixmap(QPixmap::fromImage(img));
    }

    m_scene->setSceneRect(m_image->boundingRect());
    fitImageToView();
}

void imageview::setBackground(const QImage &img)
{
    if (img.isNull())
        return;
    //还没有底图时走首次显示，需要自适应窗口
    if (!m_image || m_image->pixmap().isNull())
    {
        setImage(img);
        return;
    }

    //只换像素，不fitInView，切换原图和二值图时保持当前缩放
    m_image->setPixmap(QPixmap::fromImage(img));
    m_scene->setSceneRect(m_image->boundingRect());
}

void imageview::clearImage()
{
    if (m_image)
        m_image->setPixmap(QPixmap());

    m_scene->setSceneRect(QRectF());
    //重置视图缩放、平移
    resetTransform();
    m_zoomFactor = transform().m11();

    //禁用鼠标拖拽
    setDragMode(QGraphicsView::NoDrag);
}

void imageview::fitImageToView()
{
    if (!m_image || m_image->pixmap().isNull())
        return;

    fitInView(m_image, Qt::KeepAspectRatio);
    //获取当前真实的缩放比例
    m_zoomFactor = transform().m11();}

void imageview::setZoomRange(double min, double max)
{
    if (min <= 0.0 || max < min)
        return;

    m_minZoom = min;
    m_maxZoom = max;

    const double currentZoom = transform().m11();
    const double targetZoom = std::clamp(
        currentZoom, m_minZoom, m_maxZoom);

    if (qFuzzyCompare(currentZoom, targetZoom))
        return;

    QTransform t = transform();
    t.scale(targetZoom / currentZoom, targetZoom / currentZoom);
    setTransform(t);

    m_zoomFactor = transform().m11();
}

void imageview::setEditCallback(std::function<void()> callback)
{
    m_editCallback = std::move(callback);
}

void imageview::setDragCallback(std::function<void(QPointF)> callback)
{
    m_dragCallback = std::move(callback);
}

void imageview::setDragEndCallback(std::function<void()> callback)
{
    m_dragEndCallback = std::move(callback);
}

void imageview::translateSelection(const QPointF &delta)
{
    if (m_overlay)
        m_overlay->translateSelection(delta);
}

void imageview::rebuildOverlay()
{
    if (m_overlay)
        m_overlay->refresh();
}

void imageview::setDetection(const grain_filling::DetectResult *result, const cv::Mat *fillLabels, const std::vector<grain_filling::FillGrain> *fillGrains, const std::vector<std::vector<cv::Point>> *addedContours)
{
    if (m_overlay)
        m_overlay->setDetection(result, fillLabels, fillGrains, addedContours);
}

void imageview::clearDetection()
{
    if (m_overlay)
        m_overlay->setDetection(nullptr, nullptr, nullptr, nullptr);
}

void imageview::clearSelection()
{
    if (m_overlay)
        m_overlay->clearSelection();
}

void imageview::clearEdits()
{
    if (m_overlay)
        m_overlay->clearEdits();
}

std::vector<int> imageview::selectedLabelIds() const
{
    if (m_overlay == nullptr)
        return {};
    return m_overlay->selectedLabelIds();
}

std::vector<int> imageview::selectedAddedIndices() const
{
    if (m_overlay == nullptr)
        return {};
    return m_overlay->selectedAddedIndices();
}

bool imageview::currentFill(grain_filling::FillGrain &grain) const
{
    if (m_overlay == nullptr)
        return false;
    return m_overlay->currentFill(grain);
}

void imageview::wheelEvent(QWheelEvent *event)
{
    if (!m_image || m_image->pixmap().isNull()) {
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    constexpr double step = 1.15;
    const double factor = delta > 0 ? step : 1.0 / step;

    const double nextZoom = std::clamp(
        m_zoomFactor * factor,
        m_minZoom,
        m_maxZoom
        );

    const double actualFactor = nextZoom / m_zoomFactor;

    if (actualFactor != 1.0) {
        scale(actualFactor, actualFactor);
        m_zoomFactor = transform().m11();
    }

    event->accept();
}

void imageview::mousePressEvent(QMouseEvent *event)
{
    // setFocus();
    //中键平移。滚动条虽然隐藏，数值仍然能改
    if (event->button() == Qt::MiddleButton)
    {
        m_panning = true;
        m_lastPan = event->pos();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_overlay != nullptr)
    {
        const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
        const GrainOverlayItem::Pick pick = m_overlay->selectAt(mapToScene(event->pos()), ctrl);
        if (m_editCallback)
            m_editCallback();
        if (pick == GrainOverlayItem::Pick::Movable && !ctrl)
        {
            m_dragging = true;
            m_lastDrag = event->pos();
            event->accept();
            return;
        }
        if (pick != GrainOverlayItem::Pick::Miss)
        {
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void imageview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
    {
        const QPointF delta = mapToScene(event->pos()) - mapToScene(m_lastDrag);
        m_lastDrag = event->pos();
        if (m_dragCallback)
            m_dragCallback(delta);
        event->accept();
        return;
    }
    if (!m_panning)
    {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    //滚动条隐藏后数值仍可改，中键用这个平移
    const QPoint delta = event->pos() - m_lastPan;
    m_lastPan = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
}

void imageview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
        m_panning = false;
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        m_dragging = false;
        if (m_dragEndCallback)
            m_dragEndCallback();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

