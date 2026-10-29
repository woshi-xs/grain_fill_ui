#pragma once

#include <filesystem>

#include <QImage>

#include <opencv2/opencv.hpp>

namespace image {///
/// \brief The ImageSession class 原图路径，原图，算法检测结果图
///
struct ImageSession {
    std::string path;
    cv::Mat src;
    cv::Mat overlay;
};
}

namespace image_util {

///
/// \brief 读取图片，并规范为 3 通道 BGR
/// \param path 图片路径，UTF-8
/// \return 读取失败返回空 Mat
///
cv::Mat load(const std::filesystem::path& path);

///
/// \brief 保存图像，支持中文路径
/// \param path 保存路径
/// \param image 待保存图像
/// \return 是否保存成功
///
bool save(const std::filesystem::path& path, const cv::Mat& image);
///
/// \brief cv::Mat(BGR 3通道) 转 QImage
/// \note 返回的 QImage 与 Mat 数据独立
///
QImage matToQImage(const cv::Mat& bgrMat);

///
/// \brief QImage 转 cv::Mat(输出 3 通道 BGR)
/// \note 返回的 Mat 与 QImage 数据独立
///
cv::Mat qImageToMat(const QImage& qImg);

}