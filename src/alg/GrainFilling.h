/**
 * @file    GrainFilling.h
 * @brief   晶粒检测的数据结构和接口
 * @author  woshixs123@gmail.com
 * @date    2026-10-28
 * @version 1.0.0
 *
 * @details
 * 主要功能包括：
 * - 描述晶粒、检测参数和射线链
 * - 从原图或二值掩膜检测晶粒
 * - 按行列收链并生成缺口轮廓
 * - 用负错误码报告失败原因
 */
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <opencv2/opencv.hpp>

namespace grain_filling
{

/**
 * @class GrainInfo
 * @brief 晶粒信息结构体
 * @detail
 * - 数组下标等于 labelId，下标 0 固定为背景
 * - enable 为 false 表示软删除，不保存有效轮廓
 * - rayState 低 4 位表示上、下、左、右是否到达边界
 */
struct GrainInfo
{
    std::vector<cv::Point> contour; ///<晶粒轮廓点集
    cv::Point2d centroid;           ///<晶粒质心坐标
    int area;                       ///<连通域面积（像素）
    int length;                     ///<晶粒长，外接矩形高度（像素）
    int width;                      ///<晶粒宽，外接矩形宽度（像素）
    bool enable;                    ///<是否启用；false代表软删除
    bool inChain = false;           ///<是否已经进入过晶粒链。为true后不再进入其他链
    std::uint8_t rayState = 0; ///<四向射线状态，默认0。低4位从高到低是上、下、左、右，取值见kRayTop、kRayBottom、kRayLeft、kRayRight。
    //1100表示上和下都到达，0000表示都没到达。到达方向数用std::bitset<4>(rayState).count()。下标0和enable为false保持0，不发射线，也不挡住其他晶粒
};

constexpr std::uint8_t kRayTop = 0x08;    //1000，上边射线到达边界
constexpr std::uint8_t kRayBottom = 0x04; //0100，下边射线到达边界
constexpr std::uint8_t kRayLeft = 0x02;   //0010，左边射线到达边界
constexpr std::uint8_t kRayRight = 0x01;  //0001，右边射线到达边界

/**
 * @class DetectParam
 * @brief 检测配置参数
 * @detail
 * - 面积、长、宽的浮动倍数默认都是 1.3
 * - 晶粒较小时绝对区间很窄，可适当放大倍数
 * - labelType 传给 OpenCV 连通域，默认 CV_32S
 * @note 面积、长、宽的浮动倍数默认都是1.3。晶粒较小时绝对区间很窄，可适当放大。
 *       平均面积为8、倍数为1.3时，保留范围约为6到10，上下只差约2个像素，容易把正常晶粒剔除。
 *       长、宽同理，像素很少时1.3倍通常只差1到2个像素。
 */
struct DetectParam
{
    //图像预处理相关参数
    double thresh = 100;    ///<二值化阈值

    //连通域过滤相关参数
    int minArea = 10;       ///<连通域最小面积阈值
    int maxArea = 10000;    ///<连通域最大面积阈值
    double areaRatio = 1.3;   ///<面积上下浮动倍数。晶粒较小时可放大，见结构体@note
    double lengthRatio = 1.3; ///<长上下浮动倍数。晶粒较小时可放大，见结构体@note
    double widthRatio = 1.3;  ///<宽上下浮动倍数。晶粒较小时可放大，见结构体@note
    int connectivity = 4;   ///<连通性：4=4邻接(上下左右)，8=8邻接(含对角线)
    int labelType = CV_32S;     ///<标签图类型 CV_32S，像素值为labelId，0为背景

    //补框函数参数
    bool roundFillCount = false; ///<补粒个数是否四舍五入。false为floor(距离/step)-1，true为round(距离/step)-1
};

/**
 * @class RayGap
 * @brief 一条链上两个已有晶粒之间的缺口
 * @detail
 * - fillCentroids 的长度就是需要补的轮廓数
 * - 默认个数为 floor(距离/step)-1，roundFillCount 为 true 时改为 round 再减 1
 * - 小于 1 时不生成这个缺口
 * @note fillContours 与 fillCentroids 等长，由两端较近晶粒的轮廓平移得到。
 */
struct RayGap
{
    std::pair<int, int> boundaryLabelIds;          ///<缺口两端晶粒的labelId，按链的方向排列
    std::vector<cv::Point2d> fillCentroids;        ///<两个边界之间待生成晶粒的质心，不含两端
    std::vector<std::vector<cv::Point>> fillContours; ///<与fillCentroids等长
};

/**
 * @class GrainRayChain
 * @brief 从起点收齐的一行或一列
 * @detail
 * - labelIds 按方向有序，0010 从左到右，1000 从上到下
 * - step 是这一列相邻质心距离的最小值
 * - gaps 保存需要补轮廓的缺口
 */
struct GrainRayChain
{
    int chainId = 0;              ///<链编号，按起点顺序从0递增
    double step = 0;              ///<这一列相邻质心距离的最小值，像素
    std::vector<int> labelIds;    ///<有序晶粒labelId
    std::vector<RayGap> gaps;     ///<这一列上需要补轮廓的缺口
};

/**
 * @brief 全部晶粒链
 */
using GrainRayChainList = std::vector<GrainRayChain>;

/**
 * @class FillGrain
 * @brief 一条填补轮廓在晶粒链中的位置
 * @detail
 * - 数组下标等于填补标签图里的 labelId
 * - 下标 0 是背景占位，三个下标都为 -1
 * - 用三个下标回到链、缺口和轮廓
 */
struct FillGrain
{
    int chainIndex = -1; ///<GrainRayChainList下标
    int gapIndex = -1;   ///<该链GrainRayChain::gaps的下标
    int fillIndex = -1;  ///<该缺口RayGap::fillContours的下标
};

/**
 * @class DetectResult
 * @brief 算法输出结果
 * @detail
 * - grainList 下标对应 labelId，下标 0 是背景
 * - detect 写入灰度图和二值图，detectMask 只写入掩膜
 * - chains 保存收齐的行或列以及缺口轮廓
 */
struct DetectResult
{
    std::vector<GrainInfo> grainList; ///<晶粒列表，下标对应labelId，下标0是背景占位
    cv::Mat labels;                   ///<CV_32S标签图，像素值等于grainList下标(labelId)
    cv::Mat gray;   ///<预处理得到的灰度图。detectMask不修改它
    cv::Mat binary; ///<二值图。detect写入预处理结果，detectMask写入传入的掩膜
    double meanGrainArea = 0;   ///<通过面积硬阈值的晶粒平均面积
    double meanGrainLength = 0; ///<通过面积硬阈值的晶粒平均长
    double meanGrainWidth = 0;  ///<通过面积硬阈值的晶粒平均宽
    std::uint8_t rayState = 0;  ///<传给findGrainsByRayState的筛选位。传入0时由函数改写为0010或1000
    std::vector<int> lineStartIndices; ///<匹配射线位的晶粒下标。0010按y从上到下，1000按x从左到右
    GrainRayChainList chains;    ///<按起始晶粒收齐的行或列，含缺口质心和补上的轮廓
    std::vector<int> conflictLabelIds; ///<射线碰到、但已经在其他链里的晶粒labelId
    std::vector<int> unparticipatedLabelIds; ///<enable为true且inChain为false的晶粒labelId
};

/**
 * @brief 晶粒检测入口。预处理、过滤连通域，找出起始晶粒，再收链并补齐缺口轮廓
 * @param src const cv::Mat&。原始输入图像
 * @param param const DetectParam&。检测配置参数
 * @param[out] result DetectResult&。灰度图、二值图、标签图、晶粒列表、平均长宽、射线筛选位、起始下标、晶粒链、冲突labelId和未参与填补的labelId
 * @return int。0成功。预处理、过滤、找起始晶粒、收链或补轮廓失败时返回对应的负错误码
 */
[[nodiscard]]
int detect(const cv::Mat& src, const DetectParam& param, DetectResult& result);

/**
 * @brief 对已有二值掩膜做晶粒检测，不做预处理
 * @note 从连通域过滤开始，后续与detect相同。mask写入result.binary。不修改result.gray。
 * @param mask const cv::Mat&。输入二值掩膜，CV_8UC1
 * @param param const DetectParam&。检测配置参数。thresh不使用
 * @param[out] result DetectResult&。标签图、晶粒列表、平均长宽、射线筛选位、起始下标、晶粒链、冲突labelId和未参与填补的labelId
 * @return int。0成功。过滤、找起始晶粒、收链或补轮廓失败时返回对应的负错误码
 */
[[nodiscard]]
int detectMask(const cv::Mat& mask, const DetectParam& param, DetectResult& result);

/**
 * @brief 图像预处理：灰度转换+二值化
 * @param src const cv::Mat&。原始输入图像
 * @param thresh const double。二值化阈值
 * @param[out] gray cv::Mat&。输出灰度图
 * @param[out] binary cv::Mat&。输出二值图
 * @return int。0成功。图像为空返回-1，通道数不是1、3、4返回-2
 */
[[nodiscard]]
int preprocess(const cv::Mat& src, const double thresh, cv::Mat& gray, cv::Mat& binary);

/**
 * @brief 连通域过滤，按面积、长、宽筛选保留目标连通域
 * @param binary const cv::Mat&。输入二值图
 * @param[out] labels cv::Mat&。输出CV_32S标签图
 * @param[out] grainList std::vector<GrainInfo>&。晶粒列表，下标0为背景
 * @param[out] meanGrainArea double&。通过面积硬阈值的晶粒平均面积
 * @param[out] meanGrainLength double&。通过面积硬阈值的晶粒平均长
 * @param[out] meanGrainWidth double&。通过面积硬阈值的晶粒平均宽
 * @param minArea int。最小面积
 * @param maxArea int。最大面积
 * @param areaRatio double。面积上下浮动倍数。保留[meanGrainArea/areaRatio, meanGrainArea*areaRatio]。晶粒较小时可放大。
 *       平均面积为8、倍数为1.3时，范围约为6到10，上下只差约2个像素，容易剔除正常晶粒
 * @param connectivity int。连通域邻接方式
 * @param labelType int。labels标签图数据类型
 * @param lengthRatio double。长上下浮动倍数。保留[meanGrainLength/lengthRatio, meanGrainLength*lengthRatio]。
 *       晶粒较小时可放大，像素很少时1.3倍通常只差1到2个像素
 * @param widthRatio double。宽上下浮动倍数。保留[meanGrainWidth/widthRatio, meanGrainWidth*widthRatio]。
 *       晶粒较小时可放大，像素很少时1.3倍通常只差1到2个像素
 * @return int。0成功。二值图为空返回-3，类型不是CV_8UC1返回-4，没有有效连通域返回-5
 */
[[nodiscard]]
int filterConnectedComponents(const cv::Mat& binary,
                             cv::Mat& labels,
                             std::vector<GrainInfo>& grainList,
                             double& meanGrainArea,
                             double& meanGrainLength,
                             double& meanGrainWidth,
                             int minArea = 10,
                             int maxArea = 10000,
                             double areaRatio = 1.3,
                             int connectivity = 4,
                             int labelType = CV_32S,
                             double lengthRatio = 1.3,
                             double widthRatio = 1.3);

/**
 * @brief 按像素坐标读取标签图中的晶粒labelId
 * @param labels const cv::Mat&。CV_32S标签图，像素值等于grainList下标，0为背景
 * @param pixel const cv::Point&。像素坐标
 * @param[out] labelId int&。该像素上的labelId，背景为0
 * @return int。0成功。labels为空或类型不是CV_32S返回-6，坐标越界返回-7
 */
[[nodiscard]]
int labelIdAt(const cv::Mat& labels, const cv::Point& pixel, int& labelId);

/**
 * @brief 按晶粒平均长宽得到起始射线位
 * @note y方向是外接矩形高度length，x方向是外接矩形宽度width。
 *       y方向更长或相等时，晶粒按行从左向右补，返回kRayLeft(0010)。
 *       x方向更长时，晶粒按列从上向下补，返回kRayTop(1000)。
 * @param length double。晶粒平均长，像素
 * @param width double。晶粒平均宽，像素
 * @return std::uint8_t。0010或1000。长或宽不是有限正数时返回0
 */
std::uint8_t chooseFillDirection(double length, double width);

/**
 * @brief 从质心沿指定方向判断射线能否到达图像边界
 * @note directions为const。函数复制一份作为仍在行走的方向，循环直到副本为0。
 *       用std::bitset<4>查看低4位，下标3、2、1、0分别是上、下、左、右。
 *       水平方向固定y，按rayStep改变x；垂直方向固定x，按rayStep改变y。
 *       水平射线上下各检查rayHalfWidth行，垂直射线左右各检查rayHalfWidth列。
 *       窄带里出现其他enable为true的晶粒时，该方向停止且不记入返回值，并把该晶粒labelId写入对应的hit参数。
 *       采样点走出图像时，该方向记为到达边界，对应hit参数为-1。未请求的方向hit参数也是-1。
 *       背景、自身标签、enable为false的晶粒和下标越界的标签都不挡住。
 *       传入一个位表示单方向，四个位或在一起表示四个方向一起走。
 * @param labels const cv::Mat&。CV_32S标签图，像素值等于grainList下标，0为背景
 * @param grainList const std::vector<GrainInfo>&。晶粒列表。只用enable判断该标签是否挡住射线
 * @param center const cv::Point&。射线起点，整数像素质心
 * @param selfLabel int。当前晶粒在grainList中的下标，也是标签图中的自身标签
 * @param directions const std::uint8_t。要检测的方向，取kRayTop、kRayBottom、kRayLeft、kRayRight及其组合
 * @param[out] hitTop int&。上方向碰到的晶粒labelId，未碰到或未请求时为-1
 * @param[out] hitBottom int&。下方向碰到的晶粒labelId，未碰到或未请求时为-1
 * @param[out] hitLeft int&。左方向碰到的晶粒labelId，未碰到或未请求时为-1
 * @param[out] hitRight int&。右方向碰到的晶粒labelId，未碰到或未请求时为-1
 * @param rayHalfWidth int。射线法向半宽，实际检查厚度为2*rayHalfWidth+1
 * @param rayStep int。沿射线的采样步长，小于1时按1处理
 * @return std::uint8_t。到达边界的方向位。被挡住的方向不置位。labels为空、类型不是CV_32S或质心在图像外时返回0
 */
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
                              int rayStep);

/**
 * @brief 查找指定射线状态的晶粒并排序
 * @note rayState为0时调用chooseFillDirection(length, width)，用返回的0010或1000覆盖rayState。自动选择失败时返回-8，不改晶粒。
 *       对enable为true的晶粒调用rayReachesBorder，directions传入确定后的rayState，返回值写入GrainInfo::rayState。
 *       返回值与rayState相等才收入startIndices。下标0和enable为false的晶粒rayState写0。
 *       排序只有两种：kRayLeft(0010)按质心y从上到下升序，kRayTop(1000)按质心x从左到右升序。
 * @param labels const cv::Mat&。CV_32S标签图，像素值等于grainList下标，0为背景
 * @param grainList std::vector<GrainInfo>&。晶粒列表。射线成功后改写rayState
 * @param startIndices std::vector<int>&。输出参数，匹配并排好序的晶粒下标
 * @param rayState std::uint8_t&。筛选位。0表示自动选择，函数改写为0010或1000
 * @param length double。晶粒平均长。rayState为0时使用
 * @param width double。晶粒平均宽。rayState为0时使用
 * @param rayHalfWidth int。射线法向半宽，默认1
 * @param rayStep int。射线采样步长，默认1
 * @return int。0表示至少找到一个晶粒。labels无效返回-6，自动选择失败返回-8，筛选位低4位为0返回-9，没有匹配晶粒返回-10，下标越界返回-11
 */
[[nodiscard]]
int findGrainsByRayState(const cv::Mat& labels,
                   std::vector<GrainInfo>& grainList,
                   std::vector<int>& startIndices,
                   std::uint8_t& rayState,
                   double length,
                   double width,
                   int rayHalfWidth = 1,
                   int rayStep = 1);

/**
 * @brief 筛选指定射线状态的晶粒
 * @note 下标0不处理。GrainInfo::rayState与给定状态相等时，只把enable置为false。轮廓、质心和rayState仍保留。
 *       0010筛掉只有左边到达的晶粒，1100筛掉上和下都到达的晶粒。
 * @param grainList std::vector<GrainInfo>&。晶粒列表
 * @param rayState const std::uint8_t。要筛掉的射线状态
 * @return int。0成功
 */
[[nodiscard]]
int filterGrainsByRayState(std::vector<GrainInfo>& grainList, const std::uint8_t rayState);

/**
 * @brief 按质心坐标排序晶粒下标
 * @note 只比较质心的x或y。sortByX为true时比较x，为false时比较y。ascending为true时升序，为false时降序。
 *       坐标相等时保持原来的相对顺序。下标越界时不改动indices。
 * @param grainList const std::vector<GrainInfo>&。提供质心的晶粒列表
 * @param indices std::vector<int>&。输入待排序下标，输出排好序的下标
 * @param sortByX bool。true按x排序，false按y排序
 * @param ascending bool。true升序，false降序
 * @return int。0成功。存在越界下标时返回-11
 */
[[nodiscard]]
int sortGrainIndices(const std::vector<GrainInfo>& grainList,
                     std::vector<int>& indices,
                     bool sortByX,
                     bool ascending);

/**
 * @brief 在两个坐标之间线性插入若干点
 * @note p = start * (1 - t) + end * t，t = k / (count + 1)，k从1到count。
 *       结果按从start到end排列，不包含两个端点。count为1时只有中点。
 * @param start const cv::Point2d&。起点
 * @param end const cv::Point2d&。终点
 * @param count int。插入点个数
 * @param[out] points std::vector<cv::Point2d>&。插入的坐标
 * @return int。0成功。count小于1时清空points并返回-12
 */
[[nodiscard]]
int interpolateLine(const cv::Point2d& start,
                    const cv::Point2d& end,
                    int count,
                    std::vector<cv::Point2d>& points);

/**
 * @brief 从起始下标收集每一行或每一列，并按最小步长计算缺口
 * @note rayState为kRayLeft(0010)时向右收集，为kRayTop(1000)时向下收集。
 *       晶粒收进链时把inChain置为true。射线再碰到已在链中的晶粒时，链在它之前结束，并把该labelId记入conflictLabelIds。
 *       步长取该列相邻质心距离的最小值。
 *       相邻两颗的补齐个数默认是floor(距离/step)-1。roundFillCount为true时改为round(距离/step)-1。结果写入gaps.fillCentroids。
 * @param grainList std::vector<GrainInfo>&。晶粒列表，下标等于labelId。函数会改写inChain
 * @param labels const cv::Mat&。CV_32S标签图，像素值等于grainList下标，0为背景
 * @param startIndices const std::vector<int>&。每条链的起始晶粒下标
 * @param rayState const std::uint8_t。收集方向，只接受kRayLeft或kRayTop
 * @param[out] chains GrainRayChainList&。收集结果
 * @param[out] conflictLabelIds std::vector<int>&。射线碰到、但已经在其他链里的晶粒labelId
 * @param roundFillCount bool。true时补粒个数四舍五入，默认false，直接丢弃小数
 * @param rayHalfWidth int。沿主方向搜索下一颗时的法向半宽，默认1
 * @param rayStep int。搜索步长，默认1
 * @return int。0成功。labels无效返回-6，rayState不是0010或1000返回-13，起点下标越界返回-11，插入点失败时返回-12
 */
[[nodiscard]]
int collectGrainRayChains(std::vector<GrainInfo>& grainList,
                          const cv::Mat& labels,
                          const std::vector<int>& startIndices,
                          const std::uint8_t rayState,
                          GrainRayChainList& chains,
                          std::vector<int>& conflictLabelIds,
                          bool roundFillCount = false,
                          int rayHalfWidth = 1,
                          int rayStep = 1);

/**
 * @brief 用缺口两端较近的晶粒轮廓生成待补轮廓
 * @note 每个fillCentroid只在boundaryLabelIds的两颗晶粒里选最近的一颗。距离相等时用boundaryLabelIds.first。
 *       轮廓按整数偏移平移：offset = round(fillCentroid - 模板质心)。结果写入fillContours，不加入grainList。
 *       未参与晶粒链的是enable为true且inChain为false的晶粒，其labelId写入unparticipatedLabelIds。下标0不计入。
 * @param grainList const std::vector<GrainInfo>&。提供两端晶粒的轮廓和质心
 * @param chains GrainRayChainList&。读取每个RayGap的fillCentroids，写回fillContours
 * @param[out] unparticipatedLabelIds std::vector<int>&。enable为true且inChain为false的labelId，按下标升序
 * @return int。0成功。boundaryLabelIds越界时返回-11
 */
[[nodiscard]]
int fillRayGapContours(const std::vector<GrainInfo>& grainList,
    GrainRayChainList& chains,
    std::vector<int>& unparticipatedLabelIds);

/**
 * @brief 把填补轮廓画成标签图
 * @note fillLabels为CV_32S，尺寸等于imageSize，0是背景。fillGrains下标等于像素中的labelId，下标0是背景占位。
 *       从1起每条fillContours一个id，记下chains、gaps、fillContours三个下标。
 *       点中后取chains[chainIndex].gaps[gapIndex].fillContours[fillIndex]。
 *       空轮廓仍占labelId，但不画。轮廓重叠时后画的盖住先画的。
 * @param chains const GrainRayChainList&。已经写好fillContours的晶粒链
 * @param imageSize const cv::Size&。标签图宽高，与点击用的图像坐标一致
 * @param[out] fillLabels cv::Mat&。填补轮廓标签图
 * @param[out] fillGrains std::vector<FillGrain>&。下标对应labelId
 * @return int。0成功。imageSize宽或高小于1时返回-14，两个输出清空。没有填补轮廓时返回0，标签图全0，fillGrains只有下标0
 */
[[nodiscard]]
int buildFillLabels(const GrainRayChainList& chains,
    const cv::Size& imageSize,
    cv::Mat& fillLabels,
    std::vector<FillGrain>& fillGrains);

/**
 * @brief 一个缺口的填补个数和轮廓
 * @note 个数以count为准，不重新用步长计算。count小于1时清空fillCentroids和fillContours。
 *       插入点调用interpolateLine。模板取两端里离该质心更近的晶粒，距离相等时用boundaryLabelIds.first。
 * @param grainList const std::vector<GrainInfo>&。提供两端轮廓和质心
 * @param gap RayGap&。boundaryLabelIds必须已指向两端
 * @param count int。新的填补个数
 * @param roundFillCount bool。保留与DetectParam对齐，本函数不使用它重新决定个数
 * @return int。0成功。两端下标越界时返回-11，插入点失败时返回-12。count小于1时清空缺口并返回0
 */
[[nodiscard]]
int refillRayGap(const std::vector<GrainInfo>& grainList, RayGap& gap, int count, bool roundFillCount);

/**
 * @brief 批量设置晶粒的enable
 * @note 下标0保持enable为false，不修改。先检查全部下标，有越界时不改任何晶粒。
 * @param grainList std::vector<GrainInfo>&。晶粒列表
 * @param indices const std::vector<int>&。要设置的晶粒下标
 * @param enable bool。要写入的启用状态
 * @return int。0成功。存在越界下标时返回-11
 */
[[nodiscard]]
int setGrainsEnable(std::vector<GrainInfo>& grainList, const std::vector<int>& indices, bool enable);

} // namespace grain_filling
