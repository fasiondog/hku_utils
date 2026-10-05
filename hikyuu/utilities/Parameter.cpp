/*
 * Parameter.cpp
 *
 *  Created on: 2013-2-28
 *      Author: fasiondog
 */

#include <fmt/format.h>
#include <iterator>
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
        strcmp(value.type().name(), typeid(Datetime).name()) == 0 ||
        strcmp(value.type().name(), typeid(TimeDelta).name()) == 0 ||
#endif
        value.type() == typeid(double) || value.type() == typeid(float) ||
        strcmp(value.type().name(), typeid(std::string).name()) == 0) {
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
    } else if (strcmp(iter->second.type().name(), typeid(Datetime).name()) == 0) {
        return "Datetime";
    } else if (strcmp(iter->second.type().name(), typeid(TimeDelta).name()) == 0) {
        return "TimeDelta";
#endif
    } else if (iter->second.type() == typeid(double) || iter->second.type() == typeid(float)) {
        return "double";
    } else if (strcmp(iter->second.type().name(), typeid(std::string).name()) == 0) {
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
// 每项单独格式化后直接追加进结果，避免把已累积的整个字符串放进 format 反复重排
std::string Parameter::toString() const {
    std::string result("params[");
    auto appender = std::back_inserter(result);
    param_map_t::const_iterator iter = m_params.begin();
    for (; iter != m_params.end(); ++iter) {
        if (iter->second.type() == typeid(int64_t)) {
            fmt::format_to(appender, "{}(int): {}, ", iter->first,
                           boost::any_cast<int64_t>(iter->second));
        } else if (iter->second.type() == typeid(bool)) {
            fmt::format_to(appender, "{}(bool): {}, ", iter->first,
                           boost::any_cast<bool>(iter->second));
        } else if (iter->second.type() == typeid(double)) {
            fmt::format_to(appender, "{}(double): {}, ", iter->first,
                           boost::any_cast<double>(iter->second));
        } else if (strcmp(iter->second.type().name(), typeid(std::string).name()) == 0) {
            fmt::format_to(appender, "{}(string): {}, ", iter->first,
                           boost::any_cast<std::string>(iter->second));
#if defined(HKU_SUPPORT_DATETIME)
        } else if (strcmp(iter->second.type().name(), typeid(Datetime).name()) == 0) {
            fmt::format_to(appender, "{}(Datetime): {}, ", iter->first,
                           boost::any_cast<Datetime>(iter->second));
        } else if (strcmp(iter->second.type().name(), typeid(TimeDelta).name()) == 0) {
            fmt::format_to(appender, "{}(TimeDelta): {}, ", iter->first,
                           boost::any_cast<TimeDelta>(iter->second));
#endif
        } else {
            fmt::format_to(appender, " Unsupported({}), ", iter->second.type().name());
        }
    }
    result += "]";
    return result;
}

// 将字符串转义为带引号的 JSON 字符串字面量：'"'、'\' 与控制字符转义，
// >= 0x80 的字节按 UTF-8 约定原样透传
static std::string escapeJsonString(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 2);
    result += '"';
    for (unsigned char c : input) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (c < 0x20) {
                    fmt::format_to(std::back_inserter(result), "\\u{:04x}", c);
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }
    result += '"';
    return result;
}

// 转换为 JSON 字符串
// 键名与字符串值均为调用方输入，必须转义，防止值内的引号逃逸出 JSON 字符串或注入额外成员；
// 不支持的类型整体跳过，分隔符只在已输出的条目之间补写（尾随分隔符是非法 JSON）
std::string Parameter::toJson() const {
    std::ostringstream buf;
    buf << "{";
    size_t emitted = 0;
    auto emitKey = [&buf, &emitted](const std::string& key) {
        if (emitted++ > 0) {
            buf << ", ";
        }
        buf << escapeJsonString(key) << ": ";
    };
    param_map_t::const_iterator iter = m_params.begin();
    for (; iter != m_params.end(); ++iter) {
        if (iter->second.type() == typeid(int64_t)) {
            emitKey(iter->first);
            buf << boost::any_cast<int64_t>(iter->second);
        } else if (iter->second.type() == typeid(bool)) {
            emitKey(iter->first);
            buf << (boost::any_cast<bool>(iter->second) ? "true" : "false");
        } else if (iter->second.type() == typeid(double)) {
            emitKey(iter->first);
            buf << boost::any_cast<double>(iter->second);
        } else if (strcmp(iter->second.type().name(), typeid(std::string).name()) == 0) {
            emitKey(iter->first);
            buf << escapeJsonString(boost::any_cast<std::string>(iter->second));
#if defined(HKU_SUPPORT_DATETIME)
        } else if (strcmp(iter->second.type().name(), typeid(Datetime).name()) == 0) {
            emitKey(iter->first);
            buf << boost::any_cast<Datetime>(iter->second);
        } else if (strcmp(iter->second.type().name(), typeid(TimeDelta).name()) == 0) {
            emitKey(iter->first);
            buf << boost::any_cast<TimeDelta>(iter->second);
#endif
        } else {
            continue;
        }
    }
    buf << "}";
    return buf.str();
}

}  // namespace hku
