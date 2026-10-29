#include <alg/ImageUtil.h>

#include <fstream>
#include <vector>

#include <log/Logger.h>

namespace image_util {

cv::Mat load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        logger::easyLog().error("图片打开失败");
        return {};
    }

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0)
    {
        logger::easyLog().error("图片数据异常");
        return {};
    }

    std::vector<uchar> buffer(static_cast<size_t>(size));

    if (!file.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(size)))
    {
        logger::easyLog().error("图片读取失败");
        return {};
    }

    cv::Mat image = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

    if (image.empty())
    {
        logger::easyLog().error("图片解码失败");
        return {};
    }

    if (image.channels() == 1)
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    else if (image.channels() == 4)
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);

    return image;
}


bool save(const std::filesystem::path& path, const cv::Mat& image)
{
    if (image.empty())
        return false;

    const auto extension = path.extension().string();
    if (extension.empty())
        return false;

    std::vector<uchar> buffer;

    if (!cv::imencode(extension, image, buffer))
        return false;

    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;

    file.write(
        reinterpret_cast<const char*>(buffer.data()),
        static_cast<std::streamsize>(buffer.size())
        );

    return file.good();
}

QImage matToQImage(const cv::Mat &bgrMat)
{
    if (bgrMat.empty())
        return {};

    if(bgrMat.type() != CV_8UC3)
    {
        logger::easyLog().error("图片类型错误，目前matToQImage只支持CV_8UC3类型");
        return {};
    }

    cv::Mat rgb;
    cv::cvtColor(bgrMat, rgb, cv::COLOR_BGR2RGB);

    return QImage(
               rgb.data,
               rgb.cols,
               rgb.rows,
               static_cast<int>(rgb.step),
               QImage::Format_RGB888
               ).copy();
}

cv::Mat qImageToMat(const QImage &qImg)
{
    if (qImg.isNull())
        return {};

    QImage image = qImg.convertToFormat(QImage::Format_RGB888);

    cv::Mat rgb(
        image.height(),
        image.width(),
        CV_8UC3,
        image.bits(),
        image.bytesPerLine()
        );

    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);

    return bgr;
}

}