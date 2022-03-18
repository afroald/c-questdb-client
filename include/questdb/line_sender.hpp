/*******************************************************************************
 *     ___                  _   ____  ____
 *    / _ \ _   _  ___  ___| |_|  _ \| __ )
 *   | | | | | | |/ _ \/ __| __| | | |  _ \
 *   | |_| | |_| |  __/\__ \ |_| |_| | |_) |
 *    \__\_\\__,_|\___||___/\__|____/|____/
 *
 *  Copyright (c) 2014-2019 Appsicle
 *  Copyright (c) 2019-2022 QuestDB
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include "line_sender.h"

#include <string>
#include <string_view>
#include <stdexcept>
#include <cstdint>

namespace questdb
{
    constexpr const char* inaddr_any = "0.0.0.0";

    class line_sender;

    enum class line_sender_error_code
    {
        could_not_resolve_addr,
        invalid_api_call,
        socket_error,
        invalid_utf8,
        invalid_identifier
    };

    class line_sender_error : public std::runtime_error
    {
    public:
        line_sender_error(line_sender_error_code code, const std::string& what)
                : std::runtime_error{what}
                , _code{code}
        {}

        /** Returns 0 if there is no associated error number. */
        line_sender_error_code code() const { return _code; }

    private:
        inline static line_sender_error from_c(::line_sender_error* c_err)
        {
            line_sender_error_code code =
                static_cast<line_sender_error_code>(
                    static_cast<int>(
                        ::line_sender_error_get_code(c_err)));
            size_t c_len{0};
            const char* c_msg{::line_sender_error_msg(c_err, &c_len)};
            std::string msg{c_msg, c_len};
            line_sender_error err{code, msg};
            ::line_sender_error_free(c_err);
            return err;
        }

        template <typename F, typename... Args>
            inline static void wrapped_call(F&& f, Args&&... args)
        {
            ::line_sender_error* c_err{nullptr};
            if (!f(std::forward<Args>(args)..., &c_err))
                throw from_c(c_err);
        }

        friend class line_sender;

        line_sender_error_code _code;
    };

    class line_sender
    {
    public:
        line_sender(
            const char* host,
            const char* port,
            const char* net_interface = inaddr_any)
                : _impl{nullptr}
        {
            ::line_sender_error* c_err{nullptr};
            _impl = ::line_sender_connect(
                net_interface,
                host,
                port,
                &c_err);
            if (!_impl)
                throw line_sender_error::from_c(c_err);
        }

        line_sender(
            std::string_view host,
            std::string_view port,
            std::string_view net_interface = inaddr_any)
                : line_sender{
                    std::string{host}.c_str(),
                    std::string{port}.c_str(),
                    std::string{net_interface}.c_str()}
        {}

        line_sender(
            std::string_view host,
            uint16_t port,
            std::string_view net_interface = inaddr_any)
                : line_sender{
                    std::string{host}.c_str(),
                    std::to_string(port).c_str(),
                    std::string{net_interface}.c_str()}
        {}

        line_sender(
            const char* host,
            uint16_t port,
            const char* net_interface = inaddr_any)
                : line_sender{
                    host,
                    std::to_string(port).c_str(),
                    net_interface}
        {}

        line_sender(line_sender&& other)
            : _impl{other._impl}
        {
            if (this != &other)
                other._impl = nullptr;
        }

        line_sender& operator=(line_sender&& other)
        {
            if (this != &other)
            {
                close();
                _impl = other._impl;
                other._impl = nullptr;
            }
            return *this;
        }

        line_sender(const line_sender&) = delete;
        line_sender& operator=(const line_sender&) = delete;

        line_sender& table(std::string_view name)
        {
            line_sender_error::wrapped_call(
                ::line_sender_table,
                _impl,
                name.size(),
                name.data());
            return *this;
        }

        line_sender& symbol(std::string_view name, std::string_view value)
        {
            line_sender_error::wrapped_call(
                ::line_sender_symbol,
                _impl,
                name.size(),
                name.data(),
                value.size(),
                value.data());
            return *this;
        }

        // Require specific overloads of `column` to avoid
        // involuntary usage of the `bool` overload.
        template <typename T>
        line_sender& column(std::string_view name, T value) = delete;

        line_sender& column(std::string_view name, bool value)
        {
            line_sender_error::wrapped_call(
                ::line_sender_column_bool,
                _impl,
                name.size(),
                name.data(),
                value);
            return *this;
        }

        line_sender& column(std::string_view name, int64_t value)
        {
            line_sender_error::wrapped_call(
                ::line_sender_column_i64,
                _impl,
                name.size(),
                name.data(),
                value);
            return *this;
        }

        line_sender& column(std::string_view name, double value)
        {
            line_sender_error::wrapped_call(
                ::line_sender_column_f64,
                _impl,
                name.size(),
                name.data(),
                value);
            return *this;
        }

        line_sender& column(std::string_view name, std::string_view value)
        {
            line_sender_error::wrapped_call(
                ::line_sender_column_str,
                _impl,
                name.size(),
                name.data(),
                value.size(),
                value.data());
            return *this;
        }

        line_sender& column(std::string_view name, const char* value)
        {
            return column(name, std::string_view{value});
        }

        line_sender& column(std::string_view name, const std::string& value)
        {
            return column(name, std::string_view{value});
        }

        void at(int64_t timestamp_epoch_nanos)
        {
            line_sender_error::wrapped_call(
                ::line_sender_at,
                _impl,
                timestamp_epoch_nanos);
        }

        void at_now()
        {
            line_sender_error::wrapped_call(
                ::line_sender_at_now,
                _impl);
        }

        size_t pending_size()
        {
            return _impl
                ? ::line_sender_pending_size(_impl)
                : 0;
        }

        void flush()
        {
            line_sender_error::wrapped_call(
                ::line_sender_flush,
                _impl);
        }

        bool must_close() noexcept
        {
            return _impl
                ? ::line_sender_must_close(_impl)
                : false;
        }

        void close() noexcept
        {
            if (_impl)
            {
                ::line_sender_close(_impl);
                _impl = nullptr;
            }
        }

        ~line_sender() noexcept
        {
            close();
        }

    private:
        ::line_sender* _impl;
    };

}