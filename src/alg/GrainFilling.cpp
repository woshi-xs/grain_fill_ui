/**
 * @file    GrainFilling.cpp
 * @brief   晶粒检测实现
 * @author  私人 woshixs123@gmail.com
 * @author  工作 huangxiaosong@lvyq.com.cn
 * @date    2026-10-28
 * @version 1.0.0
 *
 * @details
 * 主要功能包括：
 * - 灰度转换和反向二值化
 * - 按面积、长、宽过滤连通域
 * - 沿射线收集晶粒链并补齐缺口
 */

#include <alg/GrainFilling.h>

#include <algorithm>
#include <bitset>
#include <cmath>

namespace grain_filling
{

int detect(const cv::Mat& src, const DetectParam& param, DetectResult& result)
{
    //图像预处理
    int res = preprocess(src, param.thresh, result.gray, result.binary);
    if (res)
    {
        return res;
    }

    return detectMask(result.binary, param, result);
}

int detectMask(const cv::Mat& mask, const DetectParam& param, DetectResult& result)
{
    result.binary = mask;

    //连通域过滤
    int res = filterConnectedComponents(result.binary,
        result.labels,
        result.grainList,
        result.meanGrainArea,
        result.meanGrainLength,
        result.meanGrainWidth,
        param.minArea,
        param.maxArea,
        param.areaRatio,
        param.connectivity,
        param.labelType,
        param.lengthRatio,
        param.widthRatio);
    if (res)
    {
        return res;
    }

    //rayState为0时按平均长宽自动选择0010或1000，再找出匹配的起始晶粒
    res = findGrainsByRayState(result.labels,
        result.grainList,
        result.lineStartIndices,
        result.rayState,
        result.meanGrainLength,
        result.meanGrainWidth);
    if (res)
    {
        return res;
    }

    //从起始晶粒收齐每一行或每一列，再把最近晶粒的轮廓平移到缺口质心
    res = collectGrainRayChains(result.grainList,
        result.labels,
        result.lineStartIndices,
        result.rayState,
        result.chains,
        result.conflictLabelIds,
        param.roundFillCount);
    if (res)
    {
        return res;
    }

    //按缺口质心平移最近晶粒的轮廓，并记下enable为true且未入链的labelId
    res = fillRayGapContours(result.grainList, result.chains, result.unparticipatedLabelIds);
    if (res)
    {
        return res;
    }

    return 0;
}


int preprocess(const cv::Mat& src, const double thresh, cv::Mat& gray, cv::Mat& binary)
{
    if (src.empty())
    {
        return -1;
    }


     //  转灰度
    if (src.channels() == 1)
    {
        gray = src.clone();
    }
    else if (src.channels() == 3)
    {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }
    else if (src.channels() == 4)
    {
        cv::Mat bgr;
        cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    }
    else
    {
        //不支持的通道
        return -2;
    }

    //反向二值化：目标晶粒为白色(255)
    cv::threshold(gray, binary, thresh, 255, cv::THRESH_BINARY_INV);
    return 0;
}

int filterConnectedComponents(const cv::Mat& binary,
    cv::Mat& labels,
    std::vector<GrainInfo>& grainList,
    double& meanGrainArea,
    double& meanGrainLength,
    double& meanGrainWidth,
    int minArea,
    int maxArea,
    double areaRatio,
    int connectivity,
    int labelType,
    double lengthRatio,
    double widthRatio)
{
    meanGrainArea = 0;
    meanGrainLength = 0;
    meanGrainWidth = 0;

    if (binary.empty())
    {
        return -3;
    }
    if (binary.type() != CV_8UC1)
    {
        return -4;
    }

    //连通域提取
    cv::Mat stats, centroids;
    const int labelCount = cv::connectedComponentsWithStats(binary, labels, stats, centroids, connectivity, labelType);

    grainList.clear();
    if (labelCount <= 1)
    {
        //只有背景，或无连通域
        return -5;
    }

    grainList.resize(labelCount);

    //硬阈值粗筛。下标从0开始，0固定为背景。
    //每颗晶粒都记录轮廓；面积不在[minArea, maxArea]的只把enable置为false。
    double areaSum = 0.0;
    double lengthSum = 0.0;
    double widthSum = 0.0;
    int hardPassCount = 0;
    for (int i = 0; i < labelCount; ++i)
    {
        if (i == 0)
        {
            grainList[0].enable = false;
            grainList[0].area = 0;
            grainList[0].length = 0;
            grainList[0].width = 0;
            grainList[0].centroid = cv::Point2d(0.0, 0.0);
            grainList[0].contour.clear();
            continue;
        }

        grainList[i].area = stats.at<int>(i, cv::CC_STAT_AREA);
        grainList[i].centroid = cv::Point2d(centroids.at<double>(i, 0), centroids.at<double>(i, 1));

        const int left = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int top = stats.at<int>(i, cv::CC_STAT_TOP);
        const int width = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int height = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        grainList[i].width = width;
        grainList[i].length = height;
        //性能优化：只在该连通域外接矩形内找轮廓，不对整张图做findContours。轮廓坐标再加回left、top。
        if (width > 0 && height > 0)
        {
            cv::Mat componentMask;
            cv::compare(labels(cv::Rect(left, top, width, height)), i, componentMask, cv::CMP_EQ);
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(componentMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (!contours.empty())
            {
                grainList[i].contour = std::move(contours[0]);
                for (cv::Point& point : grainList[i].contour)
                {
                    point.x += left;
                    point.y += top;
                }
            }
        }

        if (grainList[i].area >= minArea && grainList[i].area <= maxArea)
        {
            grainList[i].enable = true;
            areaSum += grainList[i].area;
            lengthSum += grainList[i].length;
            widthSum += grainList[i].width;
            ++hardPassCount;
        }
        else
        {
            grainList[i].enable = false;
        }
    }

    if (hardPassCount > 0)
    {
        meanGrainArea = areaSum / static_cast<double>(hardPassCount);
        meanGrainLength = lengthSum / static_cast<double>(hardPassCount);
        meanGrainWidth = widthSum / static_cast<double>(hardPassCount);
    }

    /**
     * @brief 面积上下浮动剔除
     * @note 中心是通过面积硬阈值的晶粒平均面积 meanGrainArea，不是最终保留晶粒的平均值。
     *       超出[meanGrainArea/areaRatio, meanGrainArea*areaRatio]时只把enable置为false，轮廓和长宽仍保留。
     *       晶粒较小时可适当放大areaRatio。平均面积为8、倍数为1.3时，范围约为6到10，上下只差约2个像素，容易剔除正常晶粒。
     */
    if (hardPassCount > 0 && areaRatio > 0.0)
    {
        const double minFloatArea = meanGrainArea / areaRatio;
        const double maxFloatArea = meanGrainArea * areaRatio;

        for (int i = 1; i < labelCount; ++i)
        {
            if (!grainList[i].enable)
            {
                continue;
            }

            if (grainList[i].area < minFloatArea || grainList[i].area > maxFloatArea)
            {
                grainList[i].enable = false;
            }
        }
    }

    /**
     * @brief 长上下浮动剔除
     * @note 中心是 meanGrainLength，统计口径与meanGrainArea相同。
     *       超出[meanGrainLength/lengthRatio, meanGrainLength*lengthRatio]时只把enable置为false。
     *       晶粒较小时可适当放大lengthRatio。平均长只有几个像素时，1.3倍上下通常只差1到2个像素，容易剔除正常晶粒。
     */
    if (hardPassCount > 0 && lengthRatio > 0.0)
    {
        const double minFloatLength = meanGrainLength / lengthRatio;
        const double maxFloatLength = meanGrainLength * lengthRatio;

        for (int i = 1; i < labelCount; ++i)
        {
            if (!grainList[i].enable)
            {
                continue;
            }

            if (grainList[i].length < minFloatLength || grainList[i].length > maxFloatLength)
            {
                grainList[i].enable = false;
            }
        }
    }

    /**
     * @brief 宽上下浮动剔除
     * @note 中心是 meanGrainWidth，统计口径与meanGrainArea相同。
     *       超出[meanGrainWidth/widthRatio, meanGrainWidth*widthRatio]时只把enable置为false。
     *       晶粒较小时可适当放大widthRatio。平均宽只有几个像素时，1.3倍上下通常只差1到2个像素，容易剔除正常晶粒。
     */
    if (hardPassCount > 0 && widthRatio > 0.0)
    {
        const double minFloatWidth = meanGrainWidth / widthRatio;
        const double maxFloatWidth = meanGrainWidth * widthRatio;

        for (int i = 1; i < labelCount; ++i)
        {
            if (!grainList[i].enable)
            {
                continue;
            }

            if (grainList[i].width < minFloatWidth || grainList[i].width > maxFloatWidth)
            {
                grainList[i].enable = false;
            }
        }
    }

    return 0;
}

std::uint8_t chooseFillDirection(double length, double width)
{
    if (!(length > 0.0) || !(width > 0.0) || !std::isfinite(length) || !std::isfinite(width))
    {
        return 0;
    }

    //y方向更长或相等：从左边开始。x方向更长：从上边开始。
    return length >= width ? kRayLeft : kRayTop;
}

std::uint8_t rayReachesBorder(const cv::Mat& labels,
    const std::vector<GrainInfo>& grainList,
    const cv::Point& center,
    int selfLabel,
    const std::uint8_t directions,
    int& hitTop,
    int& hitBottom,
    int& hitLeft,
    int& hitRight,
    int rayHalfWidth,
    int rayStep)
{
    hitTop = -1;
    hitBottom = -1;
    hitLeft = -1;
    hitRight = -1;
    if (labels.empty() || labels.type() != CV_32S)
    {
        return 0;
    }
    if (center.x < 0 || center.y < 0 || center.x >= labels.cols || center.y >= labels.rows)
    {
        return 0;
    }

    //directions保持不变。walking是仍在走的方向，走出图像才写入reached，碰到其他enable为true的晶粒只停住并记下labelId
    std::uint8_t walking = static_cast<std::uint8_t>(directions & (kRayTop | kRayBottom | kRayLeft | kRayRight));
    if (walking == 0)
    {
        return 0;
    }

    rayHalfWidth = std::max(0, rayHalfWidth);
    rayStep = std::max(1, rayStep);
    const int grainCount = static_cast<int>(grainList.size());
    std::uint8_t reached = 0;

    for (int delta = 0; walking != 0; delta += rayStep)
    {
        const std::bitset<4> bits(walking);
        //bitset下标3、2、1、0对应上、下、左、右
        if (bits[3])
        {
            const int y = center.y - delta;
            if (y < 0)
            {
                reached = static_cast<std::uint8_t>(reached | kRayTop);
                walking = static_cast<std::uint8_t>(walking & ~kRayTop);
            }
            else
            {
                const int x1 = std::max(0, center.x - rayHalfWidth);
                const int x2 = std::min(labels.cols - 1, center.x + rayHalfWidth);
                for (int x = x1; x <= x2; ++x)
                {
                    const int label = labels.at<int>(y, x);
                    if (label > 0 && label != selfLabel && label < grainCount && grainList[label].enable)
                    {
                        hitTop = label;
                        walking = static_cast<std::uint8_t>(walking & ~kRayTop);
                        break;
                    }
                }
            }
        }
        if (bits[2])
        {
            const int y = center.y + delta;
            if (y >= labels.rows)
            {
                reached = static_cast<std::uint8_t>(reached | kRayBottom);
                walking = static_cast<std::uint8_t>(walking & ~kRayBottom);
            }
            else
            {
                const int x1 = std::max(0, center.x - rayHalfWidth);
                const int x2 = std::min(labels.cols - 1, center.x + rayHalfWidth);
                for (int x = x1; x <= x2; ++x)
                {
                    const int label = labels.at<int>(y, x);
                    if (label > 0 && label != selfLabel && label < grainCount && grainList[label].enable)
                    {
                        hitBottom = label;
                        walking = static_cast<std::uint8_t>(walking & ~kRayBottom);
                        break;
                    }
                }
            }
        }
        if (bits[1])
        {
            const int x = center.x - delta;
            if (x < 0)
            {
                reached = static_cast<std::uint8_t>(reached | kRayLeft);
                walking = static_cast<std::uint8_t>(walking & ~kRayLeft);
            }
            else
            {
                const int y1 = std::max(0, center.y - rayHalfWidth);
                const int y2 = std::min(labels.rows - 1, center.y + rayHalfWidth);
                for (int y = y1; y <= y2; ++y)
                {
                    const int label = labels.at<int>(y, x);
                    if (label > 0 && label != selfLabel && label < grainCount && grainList[label].enable)
                    {
                        hitLeft = label;
                        walking = static_cast<std::uint8_t>(walking & ~kRayLeft);
                        break;
                    }
                }
            }
        }
        if (bits[0])
        {
            const int x = center.x + delta;
            if (x >= labels.cols)
            {
                reached = static_cast<std::uint8_t>(reached | kRayRight);
                walking = static_cast<std::uint8_t>(walking & ~kRayRight);
            }
            else
            {
                const int y1 = std::max(0, center.y - rayHalfWidth);
                const int y2 = std::min(labels.rows - 1, center.y + rayHalfWidth);
                for (int y = y1; y <= y2; ++y)
                {
                    const int label = labels.at<int>(y, x);
                    if (label > 0 && label != selfLabel && label < grainCount && grainList[label].enable)
                    {
                        hitRight = label;
                        walking = static_cast<std::uint8_t>(walking & ~kRayRight);
                        break;
                    }
                }
            }
        }
    }

    return reached;
}

int labelIdAt(const cv::Mat& labels, const cv::Point& pixel, int& labelId)
{
    labelId = -1;
    if (labels.empty() || labels.type() != CV_32S)
    {
        return -6;
    }
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= labels.cols || pixel.y >= labels.rows)
    {
        return -7;
    }

    labelId = labels.at<int>(pixel.y, pixel.x);
    return 0;
}

int findGrainsByRayState(const cv::Mat& labels,
    std::vector<GrainInfo>& grainList,
    std::vector<int>& startIndices,
    std::uint8_t& rayState,
    double length,
    double width,
    int rayHalfWidth,
    int rayStep)
{
    startIndices.clear();
    if (labels.empty() || labels.type() != CV_32S)
    {
        return -6;
    }

    if (rayState == 0)
    {
        //0表示自动选择。y更长或相等得到0010，x更长得到1000
        rayState = chooseFillDirection(length, width);
        if (rayState == 0)
        {
            return -8;
        }
    }
    if ((rayState & (kRayTop | kRayBottom | kRayLeft | kRayRight)) == 0)
    {
        return -9;
    }

    const int grainCount = static_cast<int>(grainList.size());
    for (int i = 0; i < grainCount; ++i)
    {
        grainList[i].rayState = 0;
    }

    for (int i = 1; i < grainCount; ++i)
    {
        if (!grainList[i].enable)
        {
            continue;
        }

        const cv::Point center(cvRound(grainList[i].centroid.x), cvRound(grainList[i].centroid.y));
        int hitTop = -1;
        int hitBottom = -1;
        int hitLeft = -1;
        int hitRight = -1;
        const std::uint8_t reached = rayReachesBorder(labels,
            grainList,
            center,
            i,
            rayState,
            hitTop,
            hitBottom,
            hitLeft,
            hitRight,
            rayHalfWidth,
            rayStep);
        grainList[i].rayState = reached;
        if (reached == rayState)
        {
            startIndices.push_back(i);
        }
    }

    if (startIndices.empty())
    {
        return -10;
    }

    //0010从左边界开始，按y从上到下。1000从上边界开始，按x从左到右。
    const bool sortByX = rayState == kRayTop;
    return sortGrainIndices(grainList, startIndices, sortByX, true);
}

int filterGrainsByRayState(std::vector<GrainInfo>& grainList, const std::uint8_t rayState)
{
    for (int i = 1; i < static_cast<int>(grainList.size()); ++i)
    {
        if (grainList[i].rayState == rayState)
        {
            grainList[i].enable = false;
        }
    }
    return 0;
}

int sortGrainIndices(const std::vector<GrainInfo>& grainList, std::vector<int>& indices, bool sortByX, bool ascending)
{
    const int grainCount = static_cast<int>(grainList.size());
    for (int index : indices)
    {
        if (index < 0 || index >= grainCount)
        {
            return -11;
        }
    }

    std::stable_sort(indices.begin(), indices.end(), [&](int leftIndex, int rightIndex) {
        const double first = sortByX ? grainList[leftIndex].centroid.x : grainList[leftIndex].centroid.y;
        const double second = sortByX ? grainList[rightIndex].centroid.x : grainList[rightIndex].centroid.y;
        if (first == second)
        {
            return false;
        }
        return ascending ? first < second : first > second;
    });
    return 0;
}

int interpolateLine(const cv::Point2d& start, const cv::Point2d& end, int count, std::vector<cv::Point2d>& points)
{
    points.clear();
    if (count < 1)
    {
        return -12;
    }

    points.reserve(static_cast<size_t>(count));
    for (int pointIndex = 1; pointIndex <= count; ++pointIndex)
    {
        const double t = static_cast<double>(pointIndex) / static_cast<double>(count + 1);
        points.push_back(start * (1.0 - t) + end * t);
    }
    return 0;
}

int collectGrainRayChains(std::vector<GrainInfo>& grainList,
    const cv::Mat& labels,
    const std::vector<int>& startIndices,
    const std::uint8_t rayState,
    GrainRayChainList& chains,
    std::vector<int>& conflictLabelIds,
    bool roundFillCount,
    int rayHalfWidth,
    int rayStep)
{
    chains.clear();
    conflictLabelIds.clear();
    if (labels.empty() || labels.type() != CV_32S)
    {
        return -6;
    }
    if (rayState != kRayLeft && rayState != kRayTop)
    {
        return -13;
    }

    rayHalfWidth = std::max(0, rayHalfWidth);
    rayStep = std::max(1, rayStep);
    const int grainCount = static_cast<int>(grainList.size());
    for (int index : startIndices)
    {
        if (index < 0 || index >= grainCount)
        {
            return -11;
        }
    }

    //0010向右看hitRight，1000向下看hitBottom。已经在链里的晶粒只记入冲突，不并进当前链
    const bool scanRight = rayState == kRayLeft;
    for (int start : startIndices)
    {
        if (!grainList[start].enable || grainList[start].inChain)
        {
            continue;
        }

        GrainRayChain chain;
        chain.chainId = static_cast<int>(chains.size());
        int current = start;
        chain.labelIds.push_back(current);
        grainList[current].inChain = true;
        while (true)
        {
            const cv::Point center(cvRound(grainList[current].centroid.x), cvRound(grainList[current].centroid.y));
            int hitTop = -1;
            int hitBottom = -1;
            int hitLeft = -1;
            int hitRight = -1;
            const std::uint8_t forward = scanRight ? kRayRight : kRayBottom;
            rayReachesBorder(labels,
                grainList,
                center,
                current,
                forward,
                hitTop,
                hitBottom,
                hitLeft,
                hitRight,
                rayHalfWidth,
                rayStep);
            const int next = scanRight ? hitRight : hitBottom;
            if (next < 0)
            {
                break;
            }
            if (grainList[next].inChain)
            {
                //记入冲突，同时作为当前链的终点，后面才能画出接到这颗晶粒的连线。不再改 inChain
                conflictLabelIds.push_back(next);
                chain.labelIds.push_back(next);
                break;
            }
            chain.labelIds.push_back(next);
            grainList[next].inChain = true;
            current = next;
        }

        double minStep = 0.0;
        bool hasStep = false;
        for (int i = 1; i < static_cast<int>(chain.labelIds.size()); ++i)
        {
            const cv::Point2d& previousCentroid = grainList[chain.labelIds[static_cast<size_t>(i - 1)]].centroid;
            const cv::Point2d& nextCentroid = grainList[chain.labelIds[static_cast<size_t>(i)]].centroid;
            const double distance = cv::norm(nextCentroid - previousCentroid);
            if (!hasStep || distance < minStep)
            {
                minStep = distance;
                hasStep = true;
            }
        }
        //步长取这一列相邻质心距离的最小值。个数默认floor(距离/step)-1，roundFillCount为true时改为round再减1
        chain.step = hasStep ? minStep : 0.0;

        if (chain.step > 0.0)
        {
            for (int i = 1; i < static_cast<int>(chain.labelIds.size()); ++i)
            {
                const int labelA = chain.labelIds[static_cast<size_t>(i - 1)];
                const int labelB = chain.labelIds[static_cast<size_t>(i)];
                const double distance = cv::norm(grainList[labelB].centroid - grainList[labelA].centroid);
                const double intervalCount = distance / chain.step;
                const int missingCount = static_cast<int>(roundFillCount ?
                    std::round(intervalCount) :
                    std::floor(intervalCount)) - 1;
                if (missingCount <= 0)
                {
                    continue;
                }

                RayGap gap;
                gap.boundaryLabelIds = std::make_pair(labelA, labelB);
                const int interpolated = interpolateLine(grainList[labelA].centroid,
                    grainList[labelB].centroid,
                    missingCount,
                    gap.fillCentroids);
                if (interpolated != 0)
                {
                    return interpolated;
                }
                chain.gaps.push_back(std::move(gap));
            }
        }

        chains.push_back(std::move(chain));
    }

    return 0;
}

int fillRayGapContours(const std::vector<GrainInfo>& grainList,
    GrainRayChainList& chains,
    std::vector<int>& unparticipatedLabelIds)
{
    unparticipatedLabelIds.clear();
    const int grainCount = static_cast<int>(grainList.size());
    for (GrainRayChain& chain : chains)
    {
        for (RayGap& gap : chain.gaps)
        {
            const int labelA = gap.boundaryLabelIds.first;
            const int labelB = gap.boundaryLabelIds.second;
            if (labelA < 0 || labelB < 0 || labelA >= grainCount || labelB >= grainCount)
            {
                return -11;
            }

            gap.fillContours.clear();
            gap.fillContours.reserve(gap.fillCentroids.size());
            const cv::Point2d& centroidA = grainList[static_cast<size_t>(labelA)].centroid;
            const cv::Point2d& centroidB = grainList[static_cast<size_t>(labelB)].centroid;
            for (const cv::Point2d& fillCentroid : gap.fillCentroids)
            {
                //更近的一端当模板。距离相等时用boundaryLabelIds.first
                const bool useB = cv::norm(fillCentroid - centroidB) < cv::norm(fillCentroid - centroidA);
                const int templateLabel = useB ? labelB : labelA;
                const cv::Point2d& templateCentroid = grainList[static_cast<size_t>(templateLabel)].centroid;
                const cv::Point offset(cvRound(fillCentroid.x - templateCentroid.x),
                    cvRound(fillCentroid.y - templateCentroid.y));
                std::vector<cv::Point> movedContour;
                movedContour.reserve(grainList[static_cast<size_t>(templateLabel)].contour.size());
                for (const cv::Point& point : grainList[static_cast<size_t>(templateLabel)].contour)
                {
                    movedContour.push_back(point + offset);
                }
                gap.fillContours.push_back(std::move(movedContour));
            }
        }
    }

    for (int i = 1; i < grainCount; ++i)
    {
        if (grainList[static_cast<size_t>(i)].enable && !grainList[static_cast<size_t>(i)].inChain)
        {
            unparticipatedLabelIds.push_back(i);
        }
    }
    return 0;
}

int buildFillLabels(const GrainRayChainList& chains,
    const cv::Size& imageSize,
    cv::Mat& fillLabels,
    std::vector<FillGrain>& fillGrains)
{
    fillLabels.release();
    fillGrains.clear();
    if (imageSize.width < 1 || imageSize.height < 1)
    {
        return -14;
    }

    fillLabels = cv::Mat::zeros(imageSize, CV_32S);
    fillGrains.push_back(FillGrain{});

    int labelId = 1;
    for (int chainIndex = 0; chainIndex < static_cast<int>(chains.size()); ++chainIndex)
    {
        const GrainRayChain& chain = chains[static_cast<size_t>(chainIndex)];
        for (int gapIndex = 0; gapIndex < static_cast<int>(chain.gaps.size()); ++gapIndex)
        {
            const RayGap& gap = chain.gaps[static_cast<size_t>(gapIndex)];
            for (int fillIndex = 0; fillIndex < static_cast<int>(gap.fillContours.size()); ++fillIndex)
            {
                FillGrain grain;
                grain.chainIndex = chainIndex;
                grain.gapIndex = gapIndex;
                grain.fillIndex = fillIndex;
                fillGrains.push_back(grain);

                const std::vector<cv::Point>& contour = gap.fillContours[static_cast<size_t>(fillIndex)];
                if (contour.empty())
                {
                    ++labelId;
                    continue;
                }
                cv::drawContours(fillLabels, gap.fillContours, fillIndex, cv::Scalar(labelId), cv::FILLED);
                ++labelId;
            }
        }
    }
    return 0;
}

int refillRayGap(const std::vector<GrainInfo>& grainList, RayGap& gap, int count, bool)
{
    const int grainCount = static_cast<int>(grainList.size());
    const int labelA = gap.boundaryLabelIds.first;
    const int labelB = gap.boundaryLabelIds.second;
    if (labelA < 0 || labelB < 0 || labelA >= grainCount || labelB >= grainCount)
    {
        return -11;
    }

    gap.fillCentroids.clear();
    gap.fillContours.clear();
    //个数由调用方给定。小于1时只清空，不再按步长重算
    if (count < 1)
    {
        return 0;
    }

    const int interpolated = interpolateLine(grainList[static_cast<size_t>(labelA)].centroid,
        grainList[static_cast<size_t>(labelB)].centroid,
        count,
        gap.fillCentroids);
    if (interpolated != 0)
    {
        return interpolated;
    }

    const cv::Point2d& centroidA = grainList[static_cast<size_t>(labelA)].centroid;
    const cv::Point2d& centroidB = grainList[static_cast<size_t>(labelB)].centroid;
    for (const cv::Point2d& fillCentroid : gap.fillCentroids)
    {
        //更近的一端当模板。距离相等时用boundaryLabelIds.first
        const bool useB = cv::norm(fillCentroid - centroidB) < cv::norm(fillCentroid - centroidA);
        const int templateLabel = useB ? labelB : labelA;
        const cv::Point2d& templateCentroid = grainList[static_cast<size_t>(templateLabel)].centroid;
        const cv::Point offset(cvRound(fillCentroid.x - templateCentroid.x),
            cvRound(fillCentroid.y - templateCentroid.y));
        std::vector<cv::Point> movedContour;
        movedContour.reserve(grainList[static_cast<size_t>(templateLabel)].contour.size());
        for (const cv::Point& point : grainList[static_cast<size_t>(templateLabel)].contour)
        {
            movedContour.push_back(point + offset);
        }
        gap.fillContours.push_back(std::move(movedContour));
    }
    return 0;
}

int setGrainsEnable(std::vector<GrainInfo>& grainList, const std::vector<int>& indices, bool enable)
{
    const int grainCount = static_cast<int>(grainList.size());
    for (int index : indices)
    {
        if (index < 0 || index >= grainCount)
        {
            return -11;
        }
    }

    for (int index : indices)
    {
        //下标0保持enable为false
        if (index == 0)
        {
            continue;
        }
        grainList[static_cast<size_t>(index)].enable = enable;
    }
    return 0;
}

}


