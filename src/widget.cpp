#include "widget.h"
#include "ui_widget.h"

#include "DetectParamForm.h"

#include <alg/GrainDraw.h>

#include <algorithm>

#include <QFileDialog>

#include <log/Logger.h>

Widget::Widget(QWidget *parent)
    : HWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    const QString appInfo = QStringLiteral(APP_NAME " V" APP_VERSION);
    ui->app_name->setText(appInfo);
    setWindowTitle(APP_NAME);

    logger::initEasyLog();
    logger::easyLog().info("简易日志初始化成功");

    initTopTitleBarButton();
    initLeftTitleBarButton();
    DetectParamForm::initOptions(ui);
    DetectParamForm::write(ui, m_session.param);
    m_session.param = DetectParamForm::read(ui);
    m_session.applied = m_session.param;
    //填默认值时不要把按钮点亮。之后参数一变，再按是否改了阈值决定哪个按钮可点
    DetectParamForm::bindChanges(ui, this, [this] { syncParamButtons(); });
    syncParamButtons();
    ui->imageView->setEditCallback([this] { updateContourActions(); });
    ui->imageView->setDragCallback([this](const QPointF &delta) {
        const cv::Point step = m_session.dragStep(delta.x(), delta.y());
        if (step.x == 0 && step.y == 0)
            return;
        for (int labelId : ui->imageView->selectedLabelIds())
            m_session.moveGrain(labelId, step.x, step.y);
        for (int index : ui->imageView->selectedAddedIndices())
            m_session.moveAdded(index, step.x, step.y);
        m_session.grainEdited = true;
        ui->imageView->translateSelection(QPointF(step.x, step.y));
        syncParamButtons();
    });
    ui->imageView->setDragEndCallback([this] {
        m_session.endDrag();
        ui->imageView->rebuildOverlay();
    });

    logger::easyLog().info("软件初始化成功");
}

Widget::~Widget()
{
    delete ui;
}

void Widget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        //根据bool值会改变max按钮的状态值为QIcon::off或者QIcon::on
        ui->max->setChecked(isMaximized());
    }

    QWidget::changeEvent(event);
}

void Widget::resetDetectParam()
{
    m_session.param = {};
    DetectParamForm::write(ui, m_session.param);
    m_session.param = DetectParamForm::read(ui);
    syncParamButtons();
}

void Widget::syncParamButtons()
{
    const grain_filling::DetectParam current = DetectParamForm::read(ui);
    const bool threshDirty = DetectParamForm::thresholdChanged(current, m_session.applied);
    const bool otherDirty = DetectParamForm::otherChanged(current, m_session.applied);

    ui->rebuildBinaryButton->setEnabled(threshDirty || m_session.grainEdited);
    //阈值和当前二值图不一致时，必须先生成二值图，重新执行算法保持灰色
    ui->rerunDetectButton->setEnabled(!threshDirty && (otherDirty || m_session.binaryPendingDetect));
}

void Widget::finishDetect(bool refitView, const QString &status)
{
    m_session.bakeOverlay();
    if (refitView)
    {
        m_backgroundMode = BackgroundMode::Source;
        ui->backgroundCombo->setCurrentIndex(0);
        ui->imageView->setImage(image_util::matToQImage(m_session.image.src));
    }
    else
    {
        //不调用 setImage，缩放和平移保持不动
        applyBackground();
    }
    m_session.acceptCurrentParam();
    m_session.addedContours.clear();
    m_session.grainEdited = false;
    syncParamButtons();
    ui->imageView->clearEdits();
    publishContours();
    ui->statusLabel->setText(status);
}

void Widget::reportFailure(const QString &status, const char *log)
{
    ui->statusLabel->setText(status);
    logger::easyLog().error(log);
}

void Widget::initTopTitleBarButton()
{
    ui->topTitle->setBackgroundColor(QColor(250,250,250));
    ui->max->setRoundRadius(0);
    ui->min->setRoundRadius(0);
    ui->close->setRoundRadius(0);

    ui->close->setBgHover(QColor(218,89,13));
    ui->close->setBgPressed(QColor(207,85,80));

    ui->min->setCheckable(false);
    ui->close->setCheckable(false);

    ui->max->setIcons(":/res/icons/maximize.svg",":/res/icons/restore.svg");
    ui->close->setIcon(QIcon(":/res/icons/close.svg"));
    ui->min->setIcon(QIcon(":/res/icons/min.svg"));

    ui->max->setIconSize(QSize(20, 20));
    ui->min->setIconSize(QSize(20, 20));
    ui->close->setIconSize(QSize(20, 20));
}

void Widget::initLeftTitleBarButton()
{
    ui->leftWidget->setBackgroundColor(QColor(250,250,250));
}

void Widget::on_min_clicked()
{
    this->showMinimized();
}

void Widget::on_max_clicked()
{
    if (isMaximized()) {
        ui->max->setToolTip("最大化");
        showNormal();
    } else {
        ui->max->setToolTip("恢复");
        showMaximized();
    }
}

void Widget::on_close_clicked()
{
    this->close();
}

void Widget::on_loadAndDetectButton_clicked()
{
    const QString filter = QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*.*)");
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择图片"), QString(), filter);
    if (filePath.isEmpty())
    {
        ui->statusLabel->setText(QStringLiteral("取消选择图片"));
        return;
    }

    //中文路径用宽字符交给 image_util::load
    if (!m_session.loadImage(std::filesystem::path(filePath.toStdWString()), filePath.toUtf8().toStdString()))
    {
        reportFailure(QStringLiteral("加载图片错误"), "加载图片错误");
        return;
    }

    m_session.param = DetectParamForm::read(ui);
    if (m_session.runDetect() != 0)
    {
        m_session.addedContours.clear();
        m_session.grainEdited = false;
        ui->imageView->clearEdits();
        ui->imageView->clearDetection();
        updateContourActions();
        reportFailure(QStringLiteral("检测失败"), "晶粒检测失败");
        return;
    }

    finishDetect(true, QStringLiteral("检测完成，起始晶粒：%1").arg(m_session.result.lineStartIndices.size()));
}

void Widget::on_rebuildBinaryButton_clicked()
{
    if (m_session.image.src.empty() && m_session.result.labels.empty())
    {
        ui->statusLabel->setText(QStringLiteral("请先加载图片并处理"));
        return;
    }

    if (m_session.grainEdited)
    {
        cv::Size imageSize = m_session.result.labels.size();
        if (imageSize.empty())
            imageSize = m_session.image.src.size();
        cv::Mat mask;
        if (grain_filling::exportMask(m_session.result.grainList, mask, imageSize, grain_filling::EnableDrawMode::TrueOnly, false) != 0)
        {
            ui->statusLabel->setText(QStringLiteral("生成二值化图失败"));
            return;
        }
        if (!m_session.addedContours.empty())
            cv::drawContours(mask, m_session.addedContours, -1, cv::Scalar(255), cv::FILLED);
        m_session.result.binary = mask;
        m_session.grainEdited = false;
        m_session.binaryPendingDetect = true;
        syncParamButtons();
        m_backgroundMode = BackgroundMode::Binary;
        if (ui->backgroundCombo->currentIndex() == 1)
            applyBackground();
        else
            ui->backgroundCombo->setCurrentIndex(1);
        ui->statusLabel->setText(QStringLiteral("已按晶粒轮廓生成二值化图"));
        return;
    }

    if (m_session.image.src.empty())
    {
        ui->statusLabel->setText(QStringLiteral("请先加载图片并处理"));
        return;
    }

    m_session.param.thresh = ui->binaryThresholdSpin->value();
    if (m_session.rebuildBinary() != 0)
    {
        ui->statusLabel->setText(QStringLiteral("生成二值化图失败"));
        return;
    }

    m_session.acceptNewBinary();
    syncParamButtons();
    //底图切到二值图。已经停在这一项时下拉框不会发信号，要自己刷新
    m_backgroundMode = BackgroundMode::Binary;
    if (ui->backgroundCombo->currentIndex() == 1)
        applyBackground();
    else
        ui->backgroundCombo->setCurrentIndex(1);
    ui->statusLabel->setText(QStringLiteral("已按阈值 %1 生成二值化图").arg(static_cast<int>(m_session.param.thresh)));
}

void Widget::on_rerunDetectButton_clicked()
{
    if (m_session.result.binary.empty())
    {
        ui->statusLabel->setText(QStringLiteral("请先加载图片并处理"));
        return;
    }

    m_session.param = DetectParamForm::read(ui);
    if (m_session.rerunDetect() != 0)
    {
        m_session.addedContours.clear();
        m_session.grainEdited = false;
        ui->imageView->clearEdits();
        ui->imageView->clearDetection();
        updateContourActions();
        reportFailure(QStringLiteral("重新执行算法失败"), "晶粒detectMask失败");
        return;
    }

    finishDetect(false, QStringLiteral("重新执行完成，起始晶粒：%1").arg(m_session.result.lineStartIndices.size()));
}

void Widget::on_resetParamButton_clicked()
{
    resetDetectParam();
    ui->statusLabel->setText(QStringLiteral("参数已恢复默认值"));
}

void Widget::applyBackground()
{
    cv::Mat shown;
    if (m_backgroundMode == BackgroundMode::Binary)
    {
        if (m_session.result.binary.empty())
            return;
        //matToQImage 只接受 CV_8UC3，单通道先转 BGR
        if (m_session.result.binary.channels() == 1)
            cv::cvtColor(m_session.result.binary, shown, cv::COLOR_GRAY2BGR);
        else
            shown = m_session.result.binary;
    }
    else
    {
        shown = m_session.image.src;
    }
    if (shown.empty())
        return;
    ui->imageView->setBackground(image_util::matToQImage(shown));
}

void Widget::publishContours()
{
    ui->imageView->setDetection(&m_session.result, &m_session.fillLabels, &m_session.fillGrains, &m_session.addedContours);
    updateContourActions();
}

bool Widget::isGreen(int labelId) const
{
    const std::vector<grain_filling::GrainInfo> &grains = m_session.result.grainList;
    if (labelId <= 0 || labelId >= static_cast<int>(grains.size()))
        return false;
    const grain_filling::GrainInfo &grain = grains[static_cast<size_t>(labelId)];
    if (!grain.enable || !grain.inChain)
        return false;
    const std::vector<int> &conflicts = m_session.result.conflictLabelIds;
    return std::find(conflicts.begin(), conflicts.end(), labelId) == conflicts.end();
}

bool Widget::selectedAdjacentGreens(int &chainIndex, int &labelA, int &labelB) const
{
    const std::vector<int> ids = ui->imageView->selectedLabelIds();
    if (ids.size() != 2 || !isGreen(ids[0]) || !isGreen(ids[1]))
        return false;

    const grain_filling::GrainRayChainList &chains = m_session.result.chains;
    for (int i = 0; i < static_cast<int>(chains.size()); ++i)
    {
        const std::vector<int> &labels = chains[static_cast<size_t>(i)].labelIds;
        for (int j = 1; j < static_cast<int>(labels.size()); ++j)
        {
            if (labels[static_cast<size_t>(j - 1)] == ids[0] && labels[static_cast<size_t>(j)] == ids[1])
            {
                chainIndex = i;
                labelA = ids[0];
                labelB = ids[1];
                return true;
            }
            if (labels[static_cast<size_t>(j - 1)] == ids[1] && labels[static_cast<size_t>(j)] == ids[0])
            {
                chainIndex = i;
                labelA = ids[1];
                labelB = ids[0];
                return true;
            }
        }
    }
    return false;
}

void Widget::updateContourActions()
{
    const std::vector<int> ids = ui->imageView->selectedLabelIds();
    const std::vector<int> addedIds = ui->imageView->selectedAddedIndices();
    grain_filling::FillGrain fill;
    const bool hasFill = ui->imageView->currentFill(fill);
    int greenCount = 0;
    for (int labelId : ids)
    {
        if (isGreen(labelId))
            ++greenCount;
    }
    int chainIndex = -1;
    int labelA = -1;
    int labelB = -1;
    const bool adjacent = selectedAdjacentGreens(chainIndex, labelA, labelB);

    ui->removeSelectedButton->setEnabled(!ids.empty() || !addedIds.empty());
    ui->copyContourButton->setEnabled(greenCount > 0);
    ui->insertGrainsButton->setEnabled(adjacent);
    ui->applyGapButton->setEnabled(hasFill);

    if (hasFill)
    {
        const grain_filling::RayGap &gap = m_session.result.chains[static_cast<size_t>(fill.chainIndex)].gaps[static_cast<size_t>(fill.gapIndex)];
        ui->gapCountSpin->setValue(static_cast<int>(gap.fillCentroids.size()));
    }
    ui->statusLabel->setText(selectionStatusText());
}

QString Widget::meanSizeText() const
{
    return QStringLiteral("平均长 %1，平均宽 %2")
        .arg(m_session.result.meanGrainLength, 0, 'f', 1)
        .arg(m_session.result.meanGrainWidth, 0, 'f', 1);
}

QString Widget::chainPlaceText(int labelId) const
{
    const bool rowMajor = m_session.result.rayState != grain_filling::kRayTop;
    QStringList places;
    const grain_filling::GrainRayChainList &chains = m_session.result.chains;
    for (int chainIndex = 0; chainIndex < static_cast<int>(chains.size()); ++chainIndex)
    {
        const grain_filling::GrainRayChain &chain = chains[static_cast<size_t>(chainIndex)];
        const std::vector<int> &labels = chain.labelIds;
        for (int index = 0; index < static_cast<int>(labels.size()); ++index)
        {
            if (labels[static_cast<size_t>(index)] != labelId)
                continue;
            const QString where = rowMajor
                                      ? QStringLiteral("第 %1 行第 %2 列").arg(chainIndex).arg(index)
                                      : QStringLiteral("第 %1 列第 %2 行").arg(chainIndex).arg(index);
            places << QStringLiteral("%1，该链间隔 %2").arg(where).arg(chain.step, 0, 'f', 1);
        }
    }
    if (places.isEmpty())
        return QStringLiteral("不在晶粒链中");
    return QStringLiteral("链下标从 0 开始，%1").arg(places.join(QStringLiteral("；")));
}

QString Widget::selectionStatusText() const
{
    grain_filling::FillGrain fill;
    if (ui->imageView->currentFill(fill))
    {
        const grain_filling::GrainRayChain &chain = m_session.result.chains[static_cast<size_t>(fill.chainIndex)];
        const grain_filling::RayGap &gap = chain.gaps[static_cast<size_t>(fill.gapIndex)];
        const bool rowMajor = m_session.result.rayState != grain_filling::kRayTop;
        const QString where = rowMajor
                                  ? QStringLiteral("第 %1 行").arg(fill.chainIndex)
                                  : QStringLiteral("第 %1 列").arg(fill.chainIndex);
        return QStringLiteral("红色填补。链下标从 0 开始，%1，两端 label %2 与 %3 之间，当前补了 %4 个，该链间隔 %5。%6")
            .arg(where)
            .arg(gap.boundaryLabelIds.first)
            .arg(gap.boundaryLabelIds.second)
            .arg(gap.fillCentroids.size())
            .arg(chain.step, 0, 'f', 1)
            .arg(meanSizeText());
    }

    const std::vector<int> ids = ui->imageView->selectedLabelIds();
    QStringList lines;
    const std::vector<int> &conflicts = m_session.result.conflictLabelIds;
    for (int labelId : ids)
    {
        QString kind = QStringLiteral("紫色未入链");
        if (std::find(conflicts.begin(), conflicts.end(), labelId) != conflicts.end())
            kind = QStringLiteral("蓝色冲突");
        else if (isGreen(labelId))
            kind = QStringLiteral("绿色已入链");
        lines << QStringLiteral("%1，label %2。%3。%4").arg(kind).arg(labelId).arg(chainPlaceText(labelId)).arg(meanSizeText());
    }
    const std::vector<int> addedIds = ui->imageView->selectedAddedIndices();
    for (int index : addedIds)
        lines << QStringLiteral("复制轮廓，第 %1 条（从 0 起）。不在晶粒链中。%2").arg(index).arg(meanSizeText());
    int chainIndex = -1;
    int labelA = -1;
    int labelB = -1;
    if (ids.size() == 2 && !selectedAdjacentGreens(chainIndex, labelA, labelB))
        lines << QStringLiteral("请选择同一条链上相邻的两颗绿色轮廓");
    return lines.join(QStringLiteral("\n"));
}

void Widget::on_removeSelectedButton_clicked()
{
    const std::vector<int> ids = ui->imageView->selectedLabelIds();
    std::vector<int> addedIds = ui->imageView->selectedAddedIndices();
    if (ids.empty() && addedIds.empty())
        return;
    if (!ids.empty() && grain_filling::setGrainsEnable(m_session.result.grainList, ids, false) != 0)
    {
        ui->statusLabel->setText(QStringLiteral("移除失败"));
        return;
    }
    std::sort(addedIds.begin(), addedIds.end(), std::greater<int>());
    for (int index : addedIds)
    {
        if (index < 0 || index >= static_cast<int>(m_session.addedContours.size()))
            continue;
        m_session.addedContours.erase(m_session.addedContours.begin() + index);
    }
    m_session.grainEdited = true;
    syncParamButtons();
    ui->imageView->clearSelection();
    publishContours();
    ui->statusLabel->setText(QStringLiteral("已移除选中轮廓"));
}

void Widget::on_copyContourButton_clicked()
{
    const std::vector<int> ids = ui->imageView->selectedLabelIds();
    int copied = 0;
    for (int labelId : ids)
    {
        if (!isGreen(labelId))
            continue;
        m_session.addedContours.push_back(m_session.result.grainList[static_cast<size_t>(labelId)].contour);
        ++copied;
    }
    if (copied < 1)
    {
        ui->statusLabel->setText(QStringLiteral("请选择绿色轮廓"));
        return;
    }
    m_session.grainEdited = true;
    syncParamButtons();
    publishContours();
    ui->statusLabel->setText(QStringLiteral("已复制 %1 条绿色轮廓").arg(copied));
}

void Widget::on_insertGrainsButton_clicked()
{
    int chainIndex = -1;
    int labelA = -1;
    int labelB = -1;
    if (!selectedAdjacentGreens(chainIndex, labelA, labelB))
    {
        ui->statusLabel->setText(QStringLiteral("请选择同一条链上相邻的两颗绿色轮廓"));
        return;
    }

    grain_filling::GrainRayChain &chain = m_session.result.chains[static_cast<size_t>(chainIndex)];
    grain_filling::RayGap *gap = nullptr;
    for (grain_filling::RayGap &item : chain.gaps)
    {
        if (item.boundaryLabelIds.first == labelA && item.boundaryLabelIds.second == labelB)
        {
            gap = &item;
            break;
        }
    }
    if (gap == nullptr)
    {
        chain.gaps.push_back(grain_filling::RayGap{});
        gap = &chain.gaps.back();
        gap->boundaryLabelIds = std::make_pair(labelA, labelB);
    }
    const int count = ui->insertCountSpin->value();
    if (grain_filling::refillRayGap(m_session.result.grainList, *gap, count, m_session.param.roundFillCount) != 0)
    {
        ui->statusLabel->setText(QStringLiteral("插入失败"));
        return;
    }
    if (count < 1)
    {
        chain.gaps.erase(std::remove_if(chain.gaps.begin(), chain.gaps.end(), [&](const grain_filling::RayGap &item) {
            return item.boundaryLabelIds.first == labelA && item.boundaryLabelIds.second == labelB;
        }), chain.gaps.end());
    }
    m_session.rebuildFillLabels();
    m_session.bakeOverlay();
    publishContours();
    ui->statusLabel->setText(QStringLiteral("已更新下标 %1 与 %2 之间的填补").arg(labelA).arg(labelB));
}

void Widget::on_applyGapButton_clicked()
{
    grain_filling::FillGrain fill;
    if (!ui->imageView->currentFill(fill))
        return;
    grain_filling::GrainRayChain &chain = m_session.result.chains[static_cast<size_t>(fill.chainIndex)];
    grain_filling::RayGap &gap = chain.gaps[static_cast<size_t>(fill.gapIndex)];
    const int count = ui->gapCountSpin->value();
    if (grain_filling::refillRayGap(m_session.result.grainList, gap, count, m_session.param.roundFillCount) != 0)
    {
        ui->statusLabel->setText(QStringLiteral("局部修改失败"));
        return;
    }
    if (count < 1)
    {
        chain.gaps.erase(chain.gaps.begin() + fill.gapIndex);
        ui->imageView->clearSelection();
    }
    m_session.rebuildFillLabels();
    m_session.bakeOverlay();
    publishContours();
    ui->statusLabel->setText(QStringLiteral("已把这段填补改为 %1 个").arg(count));
}

void Widget::on_backgroundCombo_currentIndexChanged(int index)
{
    //0 是原图，其余是二值图
    m_backgroundMode = index == 0 ? BackgroundMode::Source : BackgroundMode::Binary;
    applyBackground();
}
