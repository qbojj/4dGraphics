#pragma once

#include <functional>
#include <exception>
#include <string>
#include <stdarg.h>
#include <stdio.h>

namespace cpph
{
    class move_only {
    public:
        move_only() = default;
        move_only(const move_only&) = delete;
        move_only(move_only&&) = default;

        move_only &operator=(const move_only&) = delete;
        move_only &operator=(move_only&&) = default;
    };

    class destroy_helper : private move_only {
    public:
        destroy_helper() = default;
        destroy_helper( destroy_helper && ) = default;

        template<typename F>
        destroy_helper( F &&destroy ) : destr_(std::forward<F>(destroy)) {}
        ~destroy_helper() { if(destr_) destr_(); }

        destroy_helper &operator=(destroy_helper&&o)
        {
            if( this == &o ) return *this; 
            
            if( destr_ ) destr_();
            destr_ = std::move(o.destr_);

            return *this;
        }

        void clear() { destr_ = nullptr; };
    
    private:
        std::function<void()> destr_;
    };

    // calls stored function only on exception
    class exception_guard : private move_only {
    public:
        exception_guard() : count(std::uncaught_exceptions()) {};
        exception_guard( exception_guard && ) = default;
        exception_guard &operator=(exception_guard&&) = default;
        
        template<typename F>
        exception_guard( F &&onExcept ) 
            : count(std::uncaught_exceptions())
            , onExcept_(std::forward<F>(onExcept)) {}
        
        ~exception_guard()
        {
            if(onExcept_ && std::uncaught_exceptions() != count) 
                onExcept_();
        }
    
    private:
        int count;
        std::function<void()> onExcept_;
    };

    std::string cformatv(const char *fmt, va_list list)
    {
        std::string out( vsnprintf(nullptr, 0, fmt, list) + 1, '\0' );
        out.resize( vsnprintf(out.data(), out.size(), fmt, list) );

        return out;
    }

    std::string cformat(const char *fmt, ... )
    {
        va_list vl;
        va_start( vl, fmt );
        std::string out = cformatv( fmt, vl );
        va_end(vl);

        return out;
    }
}