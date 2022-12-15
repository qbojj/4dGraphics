#pragma once

#include <functional>

namespace cpph
{
    class move_only
    {
    public:
        move_only() = default;
        move_only(const move_only&) = delete;
        move_only(move_only&&) = default;

        move_only &operator=(const move_only&) = delete;
    };

    class destroy_helper : move_only
    {
    public:
        destroy_helper() = default;
        template<typename F>
        destroy_helper( F &&destroy ) : destr_(destroy) {}
        ~destroy_helper() { if(destr_) destr_(); }

        destroy_helper &operator=(destroy_helper&&o)
        {
            if( this == &o ) return *this; 
            
            if( destr_ ) destr_();
            destr_ = std::move(o.destr_); 

            return *this;
        }
    private:
        std::function<void()> destr_;
    };
}