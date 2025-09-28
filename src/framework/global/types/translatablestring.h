/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "global/translation.h"

#include "string.h"

#ifndef NO_QT_SUPPORT
#include <QString>
#endif

namespace muse {
// Context preserving replacement for the QT_*_NOOP macros.
//! Note: in order to make the string visible for `lupdate`,
//! you must write TranslatableLiteral(...) explicitly.
class TranslatableLiteral
{
public:
    constexpr TranslatableLiteral(const char* context, const char* sourceText, const char* disambiguation = nullptr,
                                  int defaultN = -1)
        : m_context{context}, m_sourceText{sourceText}, m_disambiguation{disambiguation}, m_defaultN{defaultN} {}

    constexpr const char* context() const { return m_context; }
    constexpr const char* sourceText() const { return m_sourceText; }
    constexpr const char* disambiguation() const { return m_disambiguation; }
    constexpr int defaultN() const { return m_defaultN; }

private:
    const char* m_context = nullptr;
    const char* m_sourceText = nullptr;
    const char* m_disambiguation = nullptr;
    int m_defaultN = -1;
};

//! Note: in order to make the string visible for `lupdate`,
//! you must write TranslatableString(...) explicitly.
class TranslatableString
{
public:
    const char* context = nullptr;
    String str;
    const char* disambiguation = nullptr;
    int defaultN = -1;

    TranslatableString() = default;
    /* implicit */ TranslatableString(const TranslatableLiteral& literal)
        : TranslatableString(literal.context(), literal.sourceText(), literal.disambiguation(), literal.defaultN()) {}
    TranslatableString(const char* context, const char* str, const char* disambiguation = nullptr, int n = -1)
        : context(context), str(String::fromUtf8(str)), disambiguation(disambiguation), defaultN(n) {}
    TranslatableString(const char* context, const String& str, const char* disambiguation = nullptr, int n = -1)
        : context(context), str(str), disambiguation(disambiguation), defaultN(n) {}

    static TranslatableString untranslatable(const char* str)
    {
        return TranslatableString(nullptr, str);
    }

    static TranslatableString untranslatable(const String& str)
    {
        return TranslatableString(nullptr, str);
    }

    inline bool isEmpty() const
    {
        return str.empty();
    }

    inline bool isTranslatable() const
    {
        return context && context[0];
    }

    inline String translated() const
    {
        return translated(defaultN);
    }

    inline String translated(int n) const
    {
        if (isEmpty()) {
            return {};
        }

        String res = isTranslatable() ? mtrc(context, str, disambiguation, n) : str;

        for (const auto& arg : args) {
            arg->apply(res);
        }

        return res;
    }

#ifndef NO_QT_SUPPORT
    inline QString qTranslated() const
    {
        return qTranslated(defaultN);
    }

    inline QString qTranslated(int n) const
    {
        if (isEmpty()) {
            return {};
        }

        QString res = isTranslatable() ? qtrc(context, str, disambiguation, n) : str.toQString();

        for (const auto& arg : args) {
            arg->apply(res);
        }

        return res;
    }

#endif

    template<typename ... Args>
    inline TranslatableString arg(const Args& ...) const;

    bool operator ==(const TranslatableString& other) const
    {
        return (context == other.context || (context && other.context && strcmp(context, other.context) == 0))
               && str == other.str
               && (disambiguation == other.disambiguation
                   || (disambiguation && other.disambiguation && strcmp(disambiguation, other.disambiguation) == 0))
               && defaultN == other.defaultN
               && args == other.args;
    }

    bool operator !=(const TranslatableString& other) const
    {
        return !operator==(other);
    }

private:
    struct IArg {
        virtual ~IArg() = default;

        virtual void apply(String& res) const = 0;
#ifndef NO_QT_SUPPORT
        virtual void apply(QString& res) const = 0;
#endif
    };

    template<typename ... Args>
    struct Arg;

    std::vector<std::shared_ptr<const IArg> > args;
};

template<typename ... Args>
struct TranslatableString::Arg : public TranslatableString::IArg
{
    std::tuple<Args...> args;

    Arg(const Args&... args)
        : args(args ...) {}

    void apply(String& res) const override
    {
        res = std::apply([&](const Args&... args) { return res.arg(makeArg(args)...); }, args);
    }

    template<typename T>
    static inline auto makeArg(const T& t)
    {
        if constexpr (std::is_same_v<T, TranslatableString>) {
            return t.translated();
        } else {
            return t;
        }
    }

#ifndef NO_QT_SUPPORT
    void apply(QString& res) const override
    {
        res = std::apply([&](const Args&... args) { return res.arg(makeQArg(args)...); }, args);
    }

    template<typename T>
    static inline auto makeQArg(const T& t)
    {
        if constexpr (std::is_same_v<T, TranslatableString>) {
            return t.qTranslated();
// *INDENT-OFF* // Uncrustify doesn't understand `else if constexpr`
        } else if constexpr (std::is_same_v<T, String>) {
// *INDENT-ON*
            return t.toQString();
        } else {
            return t;
        }
    }

#endif
};

template<typename Int>
struct TranslatableString::Arg<Int> : public TranslatableString::IArg
{
    const Int arg;

    Arg(Int arg) : arg(arg) {
    }

    void apply(String& res) const override
    {
        res = res.arg(arg);
    }

#ifndef NO_QT_SUPPORT
    void apply(QString& res) const override
    {
        res = res.arg(arg);
    }

#endif
};

template<>
struct TranslatableString::Arg<String> : public TranslatableString::IArg
{
    const String arg;

    template<typename ... T>
    Arg(const T& ... arg) : arg(arg ...) {
    }

    void apply(String& res) const override
    {
        res = res.arg(arg);
    }

#ifndef NO_QT_SUPPORT
    void apply(QString& res) const override
    {
        res = res.arg(arg.toQString());
    }

#endif
};

template<>
struct TranslatableString::Arg<TranslatableString> : public TranslatableString::IArg
{
    const TranslatableString arg;

    Arg(const TranslatableString& arg) : arg(arg) {
    }

    void apply(String& res) const override
    {
        res = res.arg(arg.translated());
    }

#ifndef NO_QT_SUPPORT
    void apply(QString& res) const override
    {
        res = res.arg(arg.qTranslated());
    }

#endif
};

template<typename ... Args>
TranslatableString TranslatableString::arg(const Args& ... args) const
{
    TranslatableString res = *this;
    res.args.push_back(std::make_shared<Arg<Args...> >(args ...));
    return res;
}
}
