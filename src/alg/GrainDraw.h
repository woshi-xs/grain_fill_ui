/**
 * @file    GrainDraw.h
 * @brief   晶粒轮廓、质心和结果图绘制接口
 * @author  woshixs123@gmail.com
 * @date    2026-10-28
 * @version 1.0.0
 *
 * @details
 * 主要功能包括：
 * - 按启用状态或下标绘制晶粒轮廓
 * - 绘制链上的填补轮廓和质心
 * - 导出单通道掩膜
 */
#pragma once

#include <alg/GrainFilling.h>

namespace grain_filling
{

/**
 * @class EnableDrawMode
 * @brief 画轮廓时按 m_enable 筛选
 * @detail
 * - FalseOnly 只画 m_enable 为 false 的晶粒
 * - TrueOnly 只画 m_enable 为 true 的晶粒
 * - Both 两种都画
 */
enum class EnableDrawMode
{
    FalseOnly, ///< 只画 m_enable 为 false 的晶粒
    TrueOnly,  ///< 只画 m_enable 为 true 的晶粒
    Both       ///< 两种都画
};

/**
 * @brief 在图像上绘制晶粒轮廓，绘制前转成BGR
 * @param image const cv::Mat&。输入图像。单通道按灰度转BGR，三通道按BGR使用，四通道按BGRA转BGR
 * @param[out] drawn cv::Mat&。绘制结果
 * @param grainList const std::vector<GrainInfo>&。晶粒列表，下标0为背景，直接跳过
 * @param mode EnableDrawMode。画enable为false、enable为true，或两种都画。默认只画enable为false
 * @param color const cv::Scalar&。轮廓颜色，BGR，默认绿色
 * @param thickness int。轮廓线宽，默认1
 * @return int。0成功，非0错误码。空图或通道数不是1、3、4时返回-1
 */
[[nodiscard]]
int drawGrainContours(const cv::Mat& image,
                      cv::Mat& drawn,
                      const std::vector<GrainInfo>& grainList,
                      EnableDrawMode mode = EnableDrawMode::FalseOnly,
                      const cv::Scalar& color = cv::Scalar(0, 255, 0),
                      int thickness = 1);

/**
 * @brief 按给定下标绘制晶粒轮廓
 * @param image const cv::Mat&。输入图像。单通道按灰度转BGR，三通道按BGR使用，四通道按BGRA转BGR
 * @param[out] drawn cv::Mat&。绘制结果
 * @param grainList const std::vector<GrainInfo>&。晶粒列表
 * @param indices const std::vector<int>&。要画的晶粒下标
 * @param color const cv::Scalar&。轮廓颜色，BGR，默认绿色
 * @param thickness int。轮廓线宽，默认1
 * @return int。0成功。空图、通道数不是1、3、4或下标越界时返回-1
 */
[[nodiscard]]
int drawGrainContours(const cv::Mat& image,
                      cv::Mat& drawn,
                      const std::vector<GrainInfo>& grainList,
                      const std::vector<int>& indices,
                      const cv::Scalar& color = cv::Scalar(0, 255, 0),
                      int thickness = 1);

/**
 * @brief 绘制晶粒链中填补出来的轮廓
 * @note 只画每个RayGap的fillContours。默认红色。
 * @param image const cv::Mat&。输入图像。单通道按灰度转BGR，三通道按BGR使用，四通道按BGRA转BGR
 * @param[out] drawn cv::Mat&。绘制结果
 * @param chains const GrainRayChainList&。含待补轮廓的晶粒链
 * @param color const cv::Scalar&。轮廓颜色，BGR，默认红色
 * @param thickness int。轮廓线宽，默认1
 * @return int。0成功。空图或通道数不是1、3、4时返回-1
 */
[[nodiscard]]
int drawGrainContours(const cv::Mat& image,
                      cv::Mat& drawn,
                      const GrainRayChainList& chains,
                      const cv::Scalar& color = cv::Scalar(0, 0, 255),
                      int thickness = 1);

/**
 * @brief 在指定晶粒的质心上画点
 * @note 下标来自indices，坐标来自grainList中的centroid。质心先取整再画。
 * @param image const cv::Mat&。输入图像。单通道按灰度转BGR，三通道按BGR使用，四通道按BGRA转BGR
 * @param[out] drawn cv::Mat&。绘制结果
 * @param grainList const std::vector<GrainInfo>&。提供质心的晶粒列表
 * @param indices const std::vector<int>&。要画的晶粒下标
 * @param color const cv::Scalar&。点颜色，BGR，默认红色
 * @param radius int。点半径，默认3
 * @return int。0成功。空图、通道数不是1、3、4或下标越界时返回-1
 */
[[nodiscard]]
int drawCentroids(const cv::Mat& image,
                  cv::Mat& drawn,
                  const std::vector<GrainInfo>& grainList,
                  const std::vector<int>& indices,
                  const cv::Scalar& color = cv::Scalar(0, 0, 255),
                  int radius = 3);

/**
 * @brief 导出最终结果图，并画出每条链的填补走向
 * @note 找到且已进入链的轮廓为绿色。enable为true且inChain为false的轮廓为紫色。补上的轮廓和链起始质心为红色。冲突晶粒轮廓为蓝色。
 *       走向用直线连接同一条链上相邻已有晶粒的质心，不画箭头。
 * @param image const cv::Mat&。输入图像。单通道按灰度转BGR，三通道按BGR使用，四通道按BGRA转BGR
 * @param[out] drawn cv::Mat&。绘制结果
 * @param grainList const std::vector<GrainInfo>&。晶粒列表
 * @param chains const GrainRayChainList&。晶粒链，含填补轮廓和填补质心
 * @param unparticipatedLabelIds const std::vector<int>&。未参与填补的晶粒下标
 * @param chainStartIndices const std::vector<int>&。每条链开头的晶粒下标
 * @param conflictLabelIds const std::vector<int>&。冲突晶粒下标
 * @return int。0成功。空图、通道数不是1、3、4或下标越界时返回-1
 */
[[nodiscard]]
int drawResult(const cv::Mat& image,
               cv::Mat& drawn,
               const std::vector<GrainInfo>& grainList,
               const GrainRayChainList& chains,
               const std::vector<int>& unparticipatedLabelIds,
               const std::vector<int>& chainStartIndices,
               const std::vector<int>& conflictLabelIds);

/**
 * @brief 按晶粒轮廓导出单通道掩膜
 * @param grainList const std::vector<GrainInfo>&。晶粒列表，下标0为背景，直接跳过
 * @param[out] mask cv::Mat&。输出CV_8UC1掩膜
 * @param imageSize cv::Size。掩膜尺寸。轮廓点不含画布大小，调用方传入，例如binary.size()
 * @param mode EnableDrawMode。导出enable为true、enable为false，或两种都导出。默认只导出enable为true
 * @param invert bool。为true时轮廓内部为黑、外部为白。默认false，内部白、外部黑
 * @return int。0成功，imageSize为空时返回-1
 */
[[nodiscard]]
int exportMask(const std::vector<GrainInfo>& grainList,
               cv::Mat& mask,
               cv::Size imageSize,
               EnableDrawMode mode = EnableDrawMode::TrueOnly,
               bool invert = false);

} // namespace grain_filling
