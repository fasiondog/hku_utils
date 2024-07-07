/*
 * Parameter.h
 *
 *  Created on: 2013-2-28
 *      Author: fasiondog
 */

#ifndef UTILS_PARAMETER_H_
#define UTILS_PARAMETER_H_

#pragma once

#include <boost/any.hpp>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include "yas/serialize.hpp"
#include "yas/std_types.hpp"

#if defined(HKU_SUPPORT_DATETIME)
#include "datetime/Datetime.h"
#endif

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

/**
 * 供需要命名参数设定的类使用
 * @details 在需要命名参数设定的类定义中，增加宏PARAMETER_SUPPORT，如：
 * @code
 * #//C++示例：
 * class Test {
 *     PARAMETER_SUPPORT
 *
 * public:
 *     Test();
 *     virtual ~Test();
 *
 *     void change(int n);
 *     void calculate();
 * };
 *
 * Test::Test() {
 *     addParam<int>("n", 10);
 * }
 *
 * void Test::change(int n) {
 *     setParam<int>("n", n);
 * }
 *
 * void Test::calculate() {
 *     int n = getParam<int>("n");
 *     ....
 * }
 * @endcode
 *
 * @note
 * <pre>
 * 由于Python的限制，目前只支持int、bool、double三种类型，增加新的类型支持，
 * 需要修改以下几处，对于不支持的类型将在add时直接抛出异常，请勿捕捉次异常，
 * 这样导致的程序终止，可以尽快补充需支持的类型：
 * 1、Parameter::support
 * 2、_Parameter.cpp中的AnyToPython、arameter::add<object>、Parameter::set<object>
 * 3、Parameter的序列化支持 <br/>
 * 在Python中，增加和修改参数，需要先创建或获取Parameter对象实例，通过Parameter示例
 * 造成，之后将Parameter实例直接设定修改，如：
 * 1、在init中增加参数
 *    param = Parameter();
 *    param.add("n", 1")
 *    param.add("bool", false)
 *    self.setParameter(param)
 * 2、修改参数
 *    param = x.getParameter()
 *    param.set("n", 10)
 *    param.set("bool", true)
 *    x.setParameter(param)
 * </pre>
 *
 * @ingroup Utilities
 */
class HKU_UTILS_API Parameter {
public:
    /** 构造函数 */
    Parameter() {}

    /** 赋值构造函数 */
    Parameter(const Parameter& p) : m_params(p.m_params) {}

    Parameter(Parameter&& p) : m_params(std::move(p.m_params)) {}

    /** 析构函数 */
    virtual ~Parameter() {}

    /** 赋值函数 */
    Parameter& operator=(const Parameter&);

    Parameter& operator=(Parameter&&);

    /** 判断输入对象是否属于支持的类型 */
    static bool support(const boost::any&);

    /** 获取所有参数名称列表 */
    std::vector<std::string> getNameList() const;

    /** 打印信息 */
    std::string toString() const;

    /** 输出 json 格式字符串 */
    std::string toJson() const;

    /** 是否存在指定名称的参数 */
    bool have(const std::string& name) const noexcept {
        return m_params.count(name) != 0;
    }

    /** 获取参数类型 */
    std::string type(const std::string& name) const;

    /**
     * 设定指定的参数值
     * @note 已经存在的参数修改其值，不存在的参数进行增加
     * @param name 参数名称
     * @param value 参数值
     */
    template <typename ValueType>
    void set(const std::string& name, const ValueType& value);

    /**
     * 获取指定的参数值
     * @param name 参数名
     * @return 参数值
     */
    template <typename ValueType>
    ValueType get(const std::string& name) const;

    /** 清除内容 */
    void clear() {
        m_params.clear();
    }

    /** 获取元素数量 */
    size_t size() const {
        return m_params.size();
    }

    /** 是否为空 */
    bool empty() const {
        return m_params.empty();
    }

    /**
     * 序列化保存
     * @tparam Ar
     * @param ar
     */
    template <typename Ar>
    void serialize(Ar& ar) const {
        size_t total = m_params.size();
        ar& YAS_OBJECT("count", total);
        param_map_t::const_iterator iter = m_params.begin();
        for (; iter != m_params.end(); ++iter) {
            ar& YAS_OBJECT("key", iter->first);
            if (iter->second.type() == typeid(int64_t)) {
                saveItem<Ar, int64_t>(ar, "int", iter->second);
            } else if (iter->second.type() == typeid(bool)) {
                saveItem<Ar, bool>(ar, "bool", iter->second);
            } else if (iter->second.type() == typeid(double)) {
                saveItem<Ar, double>(ar, "double", iter->second);
#if defined(__ANDROID__) && defined(__x86_64__)
            } else if (string(iter->second.type().name()) == string(typeid(std::string).name())) {
#else
            } else if (iter->second.type() == typeid(std::string)) {
#endif
                saveItem<Ar, std::string>(ar, "string", iter->second);
            }
        }
    }

    /**
     * @brief 序列化加载
     * @tparam Ar
     * @param ar
     */
    template <typename Ar>
    void serialize(Ar& ar) {
        size_t total = 0;
        ar& YAS_OBJECT("count", total);
        std::string key;
        std::string typ;
        for (size_t i = 0; i < total; i++) {
            ar& YAS_OBJECT("key", key);
            ar& YAS_OBJECT("typ", typ);
            if (typ == "int") {
                loadItem<Ar, int64_t>(ar, typ, key);
            } else if (typ == "bool") {
                loadItem<Ar, bool>(ar, typ, key);
            } else if (typ == "double") {
                loadItem<Ar, double>(ar, typ, key);
            } else if (typ == "string") {
                loadItem<Ar, std::string>(ar, typ, key);
            }
        }
    }

private:
    template <typename Ar, typename T>
    void saveItem(Ar& ar, const std::string& type, const boost::any& val) const {
        T x = boost::any_cast<T>(val);
        ar& YAS_OBJECT("typ", type);
        ar& YAS_OBJECT("val", x);
    }

    template <typename Ar, typename T>
    void loadItem(Ar& ar, const std::string& type, const std::string& key) {
        T x;
        ar& YAS_OBJECT("val", x);
        m_params[key] = x;
    }

private:
    typedef std::unordered_map<std::string, boost::any> param_map_t;
    param_map_t m_params;
};

#define PARAMETER_SUPPORT                                                                                 \
protected:                                                                                                \
    Parameter m_params;                                                                                   \
                                                                                                          \
public:                                                                                                   \
    /** 获取 Parameter */                                                                               \
    const Parameter& getParameter() const {                                                               \
        return m_params;                                                                                  \
    }                                                                                                     \
                                                                                                          \
    /** 设置 Parameter */                                                                               \
    void setParameter(const Parameter& param) {                                                           \
        m_params = param;                                                                                 \
    }                                                                                                     \
                                                                                                          \
    /** 设置 Parameter */                                                                               \
    void setParameter(Parameter&& param) {                                                                \
        m_params = std::move(param);                                                                      \
    }                                                                                                     \
                                                                                                          \
    /** 指定参数是否存在 */                                                                       \
    bool haveParam(const std::string& name) const noexcept {                                              \
        return m_params.have(name);                                                                       \
    }                                                                                                     \
                                                                                                          \
    /**                                                                                                   \
     * 设定指定参数的值, 在原本存在该参数的情况下，                                  \
     * 新设定的值类型须和原有参数类型相同，否则将抛出异常                        \
     * @param name 参数名                                                                              \
     * @param value 参数值                                                                             \
     * @exception std::logic_error                                                                        \
     */                                                                                                   \
    template <typename ValueType>                                                                         \
    void setParam(const std::string& name, const ValueType& value) {                                      \
        m_params.set<ValueType>(name, value);                                                             \
    }                                                                                                     \
                                                                                                          \
    /** 获取指定参数的值，如参数不存在或类型不匹配抛出异常 */                    \
    template <typename ValueType>                                                                         \
    ValueType getParam(const std::string& name) const {                                                   \
        return m_params.get<ValueType>(name);                                                             \
    }                                                                                                     \
                                                                                                          \
    /** 获取指定参数的值，如参数不存在或获取失败则返回指定的默认值 */        \
    template <typename ValueType>                                                                         \
    ValueType tryGetParam(const std::string& name, const ValueType& default_val) const {                  \
        ValueType result;                                                                                 \
        try {                                                                                             \
            result = m_params.get<ValueType>(name);                                                       \
        } catch (...) {                                                                                   \
            result = default_val;                                                                         \
        }                                                                                                 \
        return result;                                                                                    \
    }                                                                                                     \
                                                                                                          \
    /**                                                                                                   \
     * 从传入的 param 中，初始化指定的参数，如果 param 中不存在，则使用默认值 \
     * @param param 传入参数集                                                                       \
     * @param name 指定参数名称                                                                     \
     * @param default_value 指定参数默认值                                                         \
     */                                                                                                   \
    template <typename ValueType>                                                                         \
    void _initSingleParam(const Parameter& param, const std::string& name,                                \
                          const ValueType& default_value) {                                               \
        if (param.have(name)) {                                                                           \
            setParam<ValueType>(name, param.get<ValueType>(name));                                        \
        } else {                                                                                          \
            setParam<ValueType>(name, default_value);                                                     \
        }                                                                                                 \
    }                                                                                                     \
                                                                                                          \
    template <typename ValueType>                                                                         \
    ValueType getParamFromOther(const Parameter& other, const std::string& name,                          \
                                const ValueType& default_value) {                                         \
        if (other.have(name)) {                                                                           \
            setParam<ValueType>(name, other.get<ValueType>(name));                                        \
        } else {                                                                                          \
            setParam<ValueType>(name, default_value);                                                     \
        }                                                                                                 \
        return getParam<ValueType>(name);                                                                 \
    }

template <typename ValueType>
ValueType Parameter::get(const std::string& name) const {
    param_map_t::const_iterator iter;
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::get : " + name);
    }
    ValueType ret;
    try {
        ret = boost::any_cast<ValueType>(iter->second);
    } catch (const std::exception&) {
        throw std::logic_error("Failed convert cast: " + name);
    }
    return ret;
}

template <typename ValueType>
void Parameter::set(const std::string& name, const ValueType& value) {
    if (!have(name)) {
        if (!support(value)) {
            throw std::logic_error("Unsuport Type (" + std::string(typeid(value).name()) + ")! " +
                                   name);
        }
        m_params[name] = value;
        return;
    }

    if (m_params[name].type() != typeid(ValueType)) {
        throw std::logic_error("Mismatching type! " + name);
    }

    m_params[name] = value;
}

template <>
inline boost::any Parameter::get<boost::any>(const std::string& name) const {
    param_map_t::const_iterator iter;
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::get : " + name);
    }
    return iter->second;
}

template <>
inline int Parameter::get<int>(const std::string& name) const {
    param_map_t::const_iterator iter;
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::get : " + name);
    }
    int64_t x;
    try {
        x = boost::any_cast<int64_t>(iter->second);
    } catch (const std::exception&) {
        throw std::logic_error("Failed convert cast: " + name);
    }
    if (x > int64_t(std::numeric_limits<int>::max()) ||
        x < int64_t(std::numeric_limits<int>::min())) {
        throw std::out_of_range("out_of_range int");
    }
    return static_cast<int>(x);
}

template <>
inline int64_t Parameter::get<int64_t>(const std::string& name) const {
    param_map_t::const_iterator iter;
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::get : " + name);
    }
    int64_t ret;
    try {
        ret = boost::any_cast<int64_t>(iter->second);
    } catch (const std::exception&) {
        throw std::logic_error("Failed convert cast: " + name);
    }
    return ret;
}

template <>
inline float Parameter::get<float>(const std::string& name) const {
    param_map_t::const_iterator iter;
    iter = m_params.find(name);
    if (iter == m_params.end()) {
        throw std::out_of_range("out_of_range in Parameter::get : " + name);
    }
    double x;
    try {
        x = boost::any_cast<double>(iter->second);
    } catch (const std::exception&) {
        throw std::logic_error("Failed convert cast: " + name);
    }
    if (x > double(std::numeric_limits<float>::max()) ||
        x < double(std::numeric_limits<float>::min())) {
        throw std::out_of_range("out_of_range float");
    }
    return static_cast<float>(x);
}

template <>
inline void Parameter::set(const std::string& name, const boost::any& value) {
    if (!have(name)) {
        m_params[name] = value;
        return;
    }

    if (m_params[name].type() != value.type()) {
        throw std::logic_error("Mismatching type! need type " +
                               std::string(m_params[name].type().name()) + " but value type is " +
                               std::string(value.type().name()));
    }

    m_params[name] = value;
}

template <>
inline void Parameter::set(const std::string& name, const int& value) {
    if (!have(name)) {
        m_params[name] = static_cast<int64_t>(value);
        return;
    }

    if (m_params[name].type() != typeid(int64_t)) {
        throw std::logic_error("Mismatching type! need type " +
                               std::string(m_params[name].type().name()) +
                               " but value type is int");
    }

    m_params[name] = static_cast<int64_t>(value);
}

template <>
inline void Parameter::set(const std::string& name, const float& value) {
    if (!have(name)) {
        m_params[name] = static_cast<double>(value);
        return;
    }

    if (m_params[name].type() != typeid(double)) {
        throw std::logic_error("Mismatching type! need type " +
                               std::string(m_params[name].type().name()) +
                               " but value type is float");
    }

    m_params[name] = static_cast<double>(value);
}

} /* namespace hku */
#endif /* UTILS_PARAMETER_H_ */
