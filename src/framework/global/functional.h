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

#include <functional> // IWYU pragma: export

namespace muse {
// helper template to enable the use of overloaded lambdas
template<class ... Ts>
struct Overloaded : Ts ... {
    using Ts::operator() ...;
};

#if (__cplusplus < 202002L) && (_MSVC_LANG < 202002L)

// explicit deduction guide (not needed as of C++20)
template<class ... Ts>
Overloaded(Ts ...)->Overloaded<Ts...>;

#endif

template<typename M, typename T>
auto bindThis(M T::* memberPtr, T* thisPtr)
{
    const auto memberFn = std::mem_fn(memberPtr);

    return [=](auto&&... ts) {
        return memberFn(thisPtr, std::forward<decltype(ts)>(ts)...);
    };
}
}
