#pragma once

#include <fstream>

#include <toml++/toml.hpp>

#include <log/Logger.h>

template <typename T>
//配置父类，提供简单的加载，热重载等方法
class Config{
public:
    static T& instance() {
        static T instance;
        return instance;
    }

    /// @brief 加载配置文件
    /// @param[in] configPath 配置文件路径
    /// @return 成功返回true，失败返回false
    bool load(const std::string& configPath){
        try
        {
            m_tomlTable = toml::parse_file(configPath);
            m_configPath = configPath;
            logger::easyLog().info("{} 配置文件加载成功",m_configPath);
            return true;
        }
        catch (const toml::parse_error& err)
        {
            // 这里接入日志，打印 err.what()
            logger::easyLog().error("{}:配置文件初始化失败:{}",m_configPath,err.what());
            return false;
        }
        catch (const std::exception& e)
        {
            // 文件找不到、IO权限异常会走到这里
            logger::easyLog().error("{}:配置文件查找失败或权限不足:{}",m_configPath,e.what());
            return false;
        }
    }

    /// @brief 热重载配置，重新读取磁盘配置文件
    /// @return 成功返回true，失败返回false
    bool reload(){
        if(m_configPath.empty())
        {
            //未初始化或初始化失败
            return false;
        } 
        return this->load(m_configPath);
    }

    /// @brief 将内存中当前配置保存到本地文件
    /// @return 成功返回true，失败返回false
    bool save() const{
        if(m_configPath.empty())
        {
            //未初始化或初始化失败
            return false;
        }
        try {
            std::ofstream ofs(m_configPath, std::ios::out | std::ios::trunc);

            if (!ofs.is_open()) {
                return false;
            }

            ofs << m_tomlTable;

            if (!ofs.good()) {
                return false;
            }

            return true;
        }
        catch (const std::exception& e) {
            return false;
        }
    }

    //重载[]，让类支持[][]访问toml子节点
    toml::node_view<toml::node> operator[](std::string_view key) {
        return m_tomlTable[key];
    }
    toml::node_view<const toml::node> operator[](std::string_view key) const {
        return m_tomlTable[key];
    }

    // 拷贝、移动全部禁用，派生类会继承这条规则
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;
protected:
    Config() = default;
    virtual ~Config() = default;

    toml::table m_tomlTable;
    std::string m_configPath;
};

//AppConfig继承通用Config类
class AppConfig : public Config<AppConfig> {
private:
    AppConfig() = default;

    friend class Config<AppConfig>;
};