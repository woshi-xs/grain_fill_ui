#include "DetectSession.h"

#include <cmath>

#include <alg/GrainDraw.h>

bool DetectSession::loadImage(const std::filesystem::path &path, const std::string &utf8Path)
{
    image.path = utf8Path;
    image.src = image_util::load(path);
    return !image.src.empty();
}

int DetectSession::runDetect()
{
    result = {};
    fillLabels.release();
    fillGrains.clear();
    const int code = grain_filling::detect(image.src, param, result);
    if (code == 0)
        rebuildFillLabels();
    return code;
}

int DetectSession::rerunDetect()
{
    //detectMask 会写 result.binary。先克隆，避免清空结果后掩膜一起没了
    const cv::Mat binary = result.binary.clone();
    result = {};
    fillLabels.release();
    fillGrains.clear();
    const int code = grain_filling::detectMask(binary, param, result);
    if (code == 0)
        rebuildFillLabels();
    return code;
}

int DetectSession::rebuildBinary()
{
    return grain_filling::preprocess(image.src, param.thresh, result.gray, result.binary);
}

void DetectSession::bakeOverlay()
{
    cv::Mat resultImage;
    if (grain_filling::drawResult(image.src, resultImage, result.grainList, result.chains, result.unparticipatedLabelIds, result.lineStartIndices, result.conflictLabelIds) == 0)
        image.overlay = resultImage;
}

void DetectSession::acceptCurrentParam()
{
    applied = param;
    binaryPendingDetect = false;
}

void DetectSession::acceptNewBinary()
{
    applied.thresh = param.thresh;
    binaryPendingDetect = true;
}

void DetectSession::rebuildFillLabels()
{
    if (grain_filling::buildFillLabels(result.chains, result.labels.size(), fillLabels, fillGrains) != 0)
    {
        fillLabels.release();
        fillGrains.clear();
    }
}

cv::Point DetectSession::dragStep(double dx, double dy)
{
    m_dragRemainX += dx;
    m_dragRemainY += dy;
    const int stepX = static_cast<int>(std::trunc(m_dragRemainX));
    const int stepY = static_cast<int>(std::trunc(m_dragRemainY));
    m_dragRemainX -= stepX;
    m_dragRemainY -= stepY;
    return cv::Point(stepX, stepY);
}

void DetectSession::endDrag()
{
    m_dragRemainX = 0;
    m_dragRemainY = 0;
}

void DetectSession::moveGrain(int labelId, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;
    if (labelId <= 0 || labelId >= static_cast<int>(result.grainList.size()))
        return;
    grain_filling::GrainInfo &grain = result.grainList[static_cast<size_t>(labelId)];
    for (cv::Point &point : grain.contour)
    {
        point.x += dx;
        point.y += dy;
    }
    grain.centroid.x += dx;
    grain.centroid.y += dy;
}

void DetectSession::moveAdded(int index, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;
    if (index < 0 || index >= static_cast<int>(addedContours.size()))
        return;
    for (cv::Point &point : addedContours[static_cast<size_t>(index)])
    {
        point.x += dx;
        point.y += dy;
    }
}
