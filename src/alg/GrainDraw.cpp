/**
 * @file    GrainDraw.cpp
 * @brief   晶粒轮廓、质心、结果图和掩膜的绘制实现
 * @author  私人 woshixs123@gmail.com
 * @author  工作 huangxiaosong@lvyq.com.cn
 * @date    2026-10-28
 * @version 1.0.0
 *
 * @details
 * 主要功能包括：
 * - 把输入图转成 BGR 后再绘制
 * - 按启用状态、下标或晶粒链选择轮廓
 * - 导出填充后的单通道掩膜
 */

#include <alg/GrainDraw.h>

#include <algorithm>

namespace grain_filling
{

int drawGrainContours(const cv::Mat& image,
    cv::Mat& drawn,
    const std::vector<GrainInfo>& grainList,
    EnableDrawMode mode,
    const cv::Scalar& color,
    int thickness)
{
    //单通道按灰度、四通道按BGRA转成BGR，三通道直接使用
    if (image.empty())
    {
        drawn.release();
        return -1;
    }

    if (image.channels() == 1)
    {
        cv::cvtColor(image, drawn, cv::COLOR_GRAY2BGR);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, drawn, cv::COLOR_BGRA2BGR);
    }
    else if (image.channels() == 3)
    {
        drawn = image.clone();
    }
    else
    {
        drawn.release();
        return -1;
    }

    std::vector<std::vector<cv::Point>> contours;
    //下标0是背景，直接跳过
    for (int i = 1; i < static_cast<int>(grainList.size()); ++i)
    {
        if (grainList[i].contour.empty())
        {
            continue;
        }
        if (mode == EnableDrawMode::FalseOnly && grainList[i].enable)
        {
            continue;
        }
        if (mode == EnableDrawMode::TrueOnly && !grainList[i].enable)
        {
            continue;
        }

        contours.push_back(grainList[i].contour);
    }

    if (!contours.empty())
    {
        cv::drawContours(drawn, contours, -1, color, thickness);
    }

    return 0;
}

int drawGrainContours(const cv::Mat& image,
    cv::Mat& drawn,
    const std::vector<GrainInfo>& grainList,
    const std::vector<int>& indices,
    const cv::Scalar& color,
    int thickness)
{
    if (image.empty())
    {
        drawn.release();
        return -1;
    }

    const int grainCount = static_cast<int>(grainList.size());
    for (int index : indices)
    {
        if (index < 0 || index >= grainCount)
        {
            drawn.release();
            return -1;
        }
    }

    if (image.channels() == 1)
    {
        cv::cvtColor(image, drawn, cv::COLOR_GRAY2BGR);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, drawn, cv::COLOR_BGRA2BGR);
    }
    else if (image.channels() == 3)
    {
        drawn = image.clone();
    }
    else
    {
        drawn.release();
        return -1;
    }

    std::vector<std::vector<cv::Point>> contours;
    for (int index : indices)
    {
        if (!grainList[static_cast<size_t>(index)].contour.empty())
        {
            contours.push_back(grainList[static_cast<size_t>(index)].contour);
        }
    }

    if (!contours.empty())
    {
        cv::drawContours(drawn, contours, -1, color, thickness);
    }

    return 0;
}

int drawGrainContours(const cv::Mat& image,
    cv::Mat& drawn,
    const GrainRayChainList& chains,
    const cv::Scalar& color,
    int thickness)
{
    if (image.empty())
    {
        drawn.release();
        return -1;
    }

    if (image.channels() == 1)
    {
        cv::cvtColor(image, drawn, cv::COLOR_GRAY2BGR);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, drawn, cv::COLOR_BGRA2BGR);
    }
    else if (image.channels() == 3)
    {
        drawn = image.clone();
    }
    else
    {
        drawn.release();
        return -1;
    }

    std::vector<std::vector<cv::Point>> contours;
    for (const GrainRayChain& chain : chains)
    {
        for (const RayGap& gap : chain.gaps)
        {
            for (const std::vector<cv::Point>& contour : gap.fillContours)
            {
                if (!contour.empty())
                {
                    contours.push_back(contour);
                }
            }
        }
    }

    if (!contours.empty())
    {
        cv::drawContours(drawn, contours, -1, color, thickness);
    }

    return 0;
}

int drawCentroids(const cv::Mat& image,
    cv::Mat& drawn,
    const std::vector<GrainInfo>& grainList,
    const std::vector<int>& indices,
    const cv::Scalar& color,
    int radius)
{
    if (image.empty())
    {
        drawn.release();
        return -1;
    }

    if (image.channels() == 1)
    {
        cv::cvtColor(image, drawn, cv::COLOR_GRAY2BGR);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, drawn, cv::COLOR_BGRA2BGR);
    }
    else if (image.channels() == 3)
    {
        drawn = image.clone();
    }
    else
    {
        drawn.release();
        return -1;
    }

    const int grainCount = static_cast<int>(grainList.size());
    for (int index : indices)
    {
        if (index < 0 || index >= grainCount)
        {
            drawn.release();
            return -1;
        }
    }

    const int pointRadius = std::max(1, radius);
    for (int index : indices)
    {
        const cv::Point center(cvRound(grainList[index].centroid.x), cvRound(grainList[index].centroid.y));
        cv::circle(drawn, center, pointRadius, color, cv::FILLED);
    }
    return 0;
}

int drawResult(const cv::Mat& image,
    cv::Mat& drawn,
    const std::vector<GrainInfo>& grainList,
    const GrainRayChainList& chains,
    const std::vector<int>& unparticipatedLabelIds,
    const std::vector<int>& chainStartIndices,
    const std::vector<int>& conflictLabelIds)
{
    const cv::Scalar red(0, 0, 255);
    const cv::Scalar blue(255, 0, 0);
    const cv::Scalar purple(255, 0, 255);

    //绿色：已进入链的轮廓。紫色：enable为true且未进入链的轮廓。蓝色：冲突晶粒。红色：填补轮廓。
    if (drawGrainContours(image, drawn, grainList, EnableDrawMode::TrueOnly) != 0)
    {
        return -1;
    }
    if (drawGrainContours(drawn, drawn, grainList, unparticipatedLabelIds, purple, 1) != 0)
    {
        return -1;
    }
    if (drawGrainContours(drawn, drawn, grainList, conflictLabelIds, blue, 1) != 0)
    {
        return -1;
    }
    if (drawGrainContours(drawn, drawn, chains) != 0)
    {
        return -1;
    }

    //同一条链上相邻已有晶粒的质心连成直线，不画箭头，也不经过填补质心
    const int grainCount = static_cast<int>(grainList.size());
    for (const GrainRayChain& chain : chains)
    {
        for (int labelId : chain.labelIds)
        {
            if (labelId < 0 || labelId >= grainCount)
            {
                drawn.release();
                return -1;
            }
        }
        for (int i = 1; i < static_cast<int>(chain.labelIds.size()); ++i)
        {
            const cv::Point2d& centroidA =
                grainList[static_cast<size_t>(chain.labelIds[static_cast<size_t>(i - 1)])].centroid;
            const cv::Point2d& centroidB =
                grainList[static_cast<size_t>(chain.labelIds[static_cast<size_t>(i)])].centroid;
            const cv::Point pointA(cvRound(centroidA.x), cvRound(centroidA.y));
            const cv::Point pointB(cvRound(centroidB.x), cvRound(centroidB.y));
            cv::line(drawn, pointA, pointB, red, 1, cv::LINE_AA);
        }
    }

    //红色链起始质心。冲突晶粒已画成蓝色轮廓。
    if (drawCentroids(drawn, drawn, grainList, chainStartIndices, red, 4) != 0)
    {
        return -1;
    }
    return 0;
}

int exportMask(const std::vector<GrainInfo>& grainList,
    cv::Mat& mask,
    cv::Size imageSize,
    EnableDrawMode mode,
    bool invert)
{
    if (imageSize.empty())
    {
        mask.release();
        return -1;
    }

    std::vector<std::vector<cv::Point>> contours;
    //下标0是背景，直接跳过
    for (int i = 1; i < static_cast<int>(grainList.size()); ++i)
    {
        if (grainList[i].contour.empty())
        {
            continue;
        }
        if (mode == EnableDrawMode::FalseOnly && grainList[i].enable)
        {
            continue;
        }
        if (mode == EnableDrawMode::TrueOnly && !grainList[i].enable)
        {
            continue;
        }

        contours.push_back(grainList[i].contour);
    }

    mask = cv::Mat::zeros(imageSize, CV_8UC1);
    if (!contours.empty())
    {
        cv::drawContours(mask, contours, -1, cv::Scalar(255), cv::FILLED);
    }

    if (invert)
    {
        cv::bitwise_not(mask, mask);
    }

    return 0;
}

} // namespace grain_filling
