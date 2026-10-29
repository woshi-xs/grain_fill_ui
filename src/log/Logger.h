#pragma once

#include <spdlog/spdlog.h>
//日志等级分为：trace、debug、info、warning、error、critical、off，其中off为设置的日志等级为非法等级
namespace logger
{

    /// @brief 初始化日志配置
    /// @param[in] name         logger 名称
    /// @param[in] logPath      日志文件路径
    /// @param[in] maxFileSize  单日志文件最大字节
    /// @param[in] maxFiles     日志文件最大数量
    /// @param[in] logLevel     日志等级
    /// @return 创建好的 Logger
    std::shared_ptr<spdlog::logger> createLogger(const std::string &logPath,int maxFileSize,int maxFiles,spdlog::level::level_enum logLevel);

    /// @brief 初始化日志
    /// @return 0成功 非0失败,失败时easylog自动启用，可通过 easylog()-> 使用简单日志，等级info
    int init(int maxFileSize,int maxFiles,spdlog::level::level_enum logLevel);

    /// @brief 简易日志，用于正式的日志库未初始化时的紧急日志，用于排查紧急错误
    void initEasyLog();

    // 简单封装，方便后续调用和调试
    inline spdlog::logger& easyLog()
    {
        auto ptrEasyLog = spdlog::get("easyLog");
        if(!ptrEasyLog){
             throw std::runtime_error("easyLog logger返回为空,请检查是否初始化和spdlog::get(\"easylog\")字段是否正确");
        }
        return *ptrEasyLog;
    }

    inline spdlog::logger& systemLog()
    {
        auto ptrSystemLog = spdlog::get("systemLog");
        if(!ptrSystemLog){
             throw std::runtime_error("systemLog logger返回为空,请检查是否初始化和spdlog::get(\"doraemon\")字段是否正确");
        }
        return *ptrSystemLog;
    }
}