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
#pragma once

#include <utility>

#include "global/thirdparty/expected/expected.hpp"

namespace muse {
template<typename T, typename E>
using Expected = tl::expected<T, E>;

// class template argument deduction for aggregates and aliases is a C++20 feature
#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L

template<typename E>
using Unexpected = tl::ununexpected<E>;

#else

template<typename E>
tl::unexpected<E> Unexpected(E&& e)
{
    return tl::unexpected(std::forward<E>(e));
}

#endif

#define RETURN_UNEXPECTED(e) \
    if (!e) { \
        return ::muse::Unexpected(std::move(e.error())); \
    }
} // namespace muse
