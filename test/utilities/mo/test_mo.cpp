
/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-19
 *      Author: fasiondog
 */

#include "test_config.h"
#include <hikyuu/utilities/mo/mo.h>

using namespace hku;

TEST_CASE("test_mo") {
    mo::init();
    auto sys_lang = mo::getSystemLanguage();
    HKU_INFO("系统语言：{}", sys_lang);
}
