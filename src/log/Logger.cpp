#include <log/Logger.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <config/Config.h>
namespace logger
{
    std::shared_ptr<spdlog::logger> createLogger(const std::string& name,const std::string &logPath,int maxFileSize,int maxFiles,spdlog::level::level_enum logLevel)
    {
        //控制台输出
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
       
        //滚动日志文件
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logPath,
            maxFileSize,
            maxFiles
        );

        //创建 logger
        auto logger = std::make_shared<spdlog::logger>(
            name,
            spdlog::sinks_init_list
            {
                console_sink,
                file_sink
            }
        );


        //设置日志等级
        logger->set_level(logLevel);

        //设置日志格式
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        // info 及以上日志立即 flush
        logger->flush_on(spdlog::level::info);

        return logger;
    }

    int init(int maxFileSize_MB,int maxFiles,spdlog::level::level_enum logLevel)
    {
        maxFileSize_MB *= 1024*1024;
        //创建系统日志
        auto systemLog = createLogger("doraemon","log/systemLog.log",maxFileSize_MB,maxFiles,logLevel);

        //注册系统日志
        spdlog::register_logger(systemLog);

        return 0;
    }

    void initEasyLog()
    {
        //创建简单日志，用于其他日志未初始化时使用
        auto easyLog = createLogger("easyLog","log/easyLog.log",1*1024*1024,5,spdlog::level::info);
        //注册logger
        spdlog::register_logger(easyLog);
    }
}