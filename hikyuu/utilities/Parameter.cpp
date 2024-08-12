/*
 * Parameter.cpp
 *
 *  Created on: 2013-2-28
 *      Author: fasiondog
 */

#include <fmt/format.h>
#include "Parameter.h"

namespace hku {

Parameter& Parameter::operator=(const Parameter& p) {
    if (this == &p) {
        return *this;
    }

    m_params = p.m_params;
    return *this;
}

Parameter& Parameter::operator=(Parameter&& p) {
    if (this == &p) {
        return *this;
    }

    m_params = std::move(p.m_params);
    return *this;
}

// 检测 any 对象是否是支持的对象类型
bool Parameter::support(const boost::any& value) {
    if (value.type() == typeid(int) || value.type() == typeid(bool) ||
        value.type() == typeid(int64_t) ||
#if defined(HKU_SUPPORT_DATETIME)
#if defined(__ANDROID__) || defined(__clang__)
        std::string(value.type().name()) == std::string(typeid(Datetime).name()) ||
        std::string(value.type().name()) == std::string(typeid(TimeDelta).name()) ||
#else
        value.type() == typeid(Datetime) || value.type() == typeid(TimeDelta) ||
#endif
#endif
        value.type() == typeid(double) ||
        value.type() == typeid(float)
#if defined(__ANDROID__)
        // android x86 64位 和 android 5 下 string 的 typeid 不正确，只能特殊处理
        || std::string(value.type().name()) == std::string(typeid(std::string).name())) {
#else
        || value.type() == typeid(std::string)) {
#endif
        return true;
    }
    fmt::print("type name: {}\n", value.type().name());
    return false;
}

std::string Parameter::type(const std::string& name) const {
    param_map_t::const_iterator iter;

    // 检测参数是否存在，否则抛出异常
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::type : " + name);
    }

    if (iter->second.type() == typeid(int) || iter->second.type() == typeid(int64_t)) {
        return "int";
    } else if (iter->second.type() == typeid(bool)) {
        return "bool";
#if defined(HKU_SUPPORT_DATETIME)
    } else if (iter->second.type() == typeid(Datetime)) {
        return "Datetime";
    } else if (iter->second.type() == typeid(TimeDelta)) {
        return "TimeDelta";
#endif
    } else if (iter->second.type() == typeid(double) || iter->second.type() == typeid(float)) {
        return "double";
#if defined(__ANDROID__)
    } else if (std::string(iter->second.type().name()) == std::string(typeid(std::string).name())) {
#else
    } else if (iter->second.type() == typeid(std::string)) {
#endif
        return "string";
    }

    return "Unknow";
}

// 获取所有的参数名
std::vector<std::string> Parameter::getNameList() const {
    std::vector<std::string> result;
    param_map_t::const_iterator iter = m_params.begin();
    for (; iter != m_params.end(); ++iter) {
        result.push_back(iter->first);
    }
    return result;
}

// 转换为字符串，用于打印输出
std::string Parameter::toString() const {
    std::string result("params[");
    param_map_t::const_iterator iter = m_params.begin();
    for (; iter != m_params.end(); ++iter) {
        if (iter->second.type() == typeid(int64_t)) {
            result = fmt::format("{}{}(int): {}, ", result, iter->first,
                                 boost::any_cast<int64_t>(iter->second));
        } else if (iter->second.type() == typeid(bool)) {
            result = fmt::format("{}{}(bool): {}, ", result, iter->first,
                                 boost::any_cast<bool>(iter->second));
        } else if (iter->second.type() == typeid(double)) {
            result = fmt::format("{}{}(double): {}, ", result, iter->first,
                                 boost::any_cast<double>(iter->second));
#if defined(__ANDROID__)
        } else if (std::string(iter->second.type().name()) ==
                   std::string(typeid(std::string).name())) {
#else
        } else if (iter->second.type() == typeid(std::string)) {
#endif
            result = fmt::format("{}{}(string): {}, ", result, iter->first,
                                 boost::any_cast<std::string>(iter->second));
        } else {
            result = fmt::format("{} Unsupported({}), ", result, iter->second.type().name());
        }
    }
    result = fmt::format("{}]", result);
    return result;
}

// 转换为字符串，用于打印输出
std::string Parameter::toJson() const {
    std::ostringstream buf;
    buf << "{";
    param_map_t::const_iterator iter = m_params.begin();
    for (size_t cnt = 0, total = m_params.size(); iter != m_params.end(); ++iter, cnt++) {
        if (iter->second.type() == typeid(int64_t)) {
            buf << "\"" << iter->first << "\": " << boost::any_cast<int64_t>(iter->second);
        } else if (iter->second.type() == typeid(bool)) {
            buf << "\"" << iter->first
                << "\": " << (boost::any_cast<bool>(iter->second) ? "true" : "false");
        } else if (iter->second.type() == typeid(double)) {
            buf << "\"" << iter->first << "\": " << boost::any_cast<double>(iter->second);
#if defined(__ANDROID__)
        } else if (std::string(iter->second.type().name()) ==
                   std::string(typeid(std::string).name())) {
#else
        } else if (iter->second.type() == typeid(std::string)) {
#endif
            buf << "\"" << iter->first << "\": \"" << boost::any_cast<std::string>(iter->second)
                << "\"";
        } else {
            continue;
        }
        if (cnt + 1 < total) {
            buf << ", ";
        }
    }
    buf << "}";
    return buf.str();
}

}  // namespace hku
