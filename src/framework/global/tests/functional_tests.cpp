/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "global/functional.h"

using namespace muse;

using ::testing::Return;

TEST(FunctionalUtilsTests, testBindThis)
{
    struct MockClass {
        MOCK_METHOD(void, noRetNoParam, ());
        MOCK_METHOD(bool, noParam, ());
        MOCK_METHOD(bool, oneParam, (int p));
        MOCK_METHOD(bool, moreParams, (bool p1, float p2, int p3));
    };

    {
        MockClass mock{};
        EXPECT_CALL(mock, noRetNoParam());

        const auto noRetNoParam = bindThis(&MockClass::noRetNoParam, &mock);
        noRetNoParam();
    }
    {
        MockClass mock{};
        EXPECT_CALL(mock, noParam())
        .WillOnce(Return(true));

        const auto noParam = bindThis(&MockClass::noParam, &mock);
        EXPECT_TRUE(noParam());
    }
    {
        MockClass mock{};
        EXPECT_CALL(mock, oneParam(42))
        .WillOnce(Return(true));

        const auto oneParam = bindThis(&MockClass::oneParam, &mock);
        EXPECT_TRUE(oneParam(42));
    }
    {
        MockClass mock{};
        EXPECT_CALL(mock, moreParams(true, 3.14f, 42))
        .WillOnce(Return(true));

        const auto moreParams = bindThis(&MockClass::moreParams, &mock);
        EXPECT_TRUE(moreParams(true, 3.14f, 42));
    }
}
