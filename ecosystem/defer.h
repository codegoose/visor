#pragma once

#include <functional>

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a,b)

// The specified function will be executed upon leaving this scope.
#define DEFER(fn) sc::scope_guard CONCAT(__defer__, __LINE__) = [&] ( ) { fn ; }

/*

Works like the Go keyword: https://gobyexample.com/defer
Borrowed from: https://oded.dev/2017/10/05/go-defer-in-cpp/

~~~

Defer is used to ensure that a function call is performed later
in a program’s execution, usually for purposes of cleanup. defer
is often used where e.g. ensure and finally would be used in other
languages.

~~~

Examples:

    DEFER(curl_global_cleanup());

    DEFER({
        spdlog::info("Deleting NVG context...");
        nvgDeleteGL3(vg);
    });

*/

namespace sc {
    class scope_guard {
        public:
            template<class callable>
            scope_guard(callable &&fn) : fn_(std::forward<callable>(fn)) {}
            scope_guard(scope_guard &&other) : fn_(std::move(other.fn_)) { other.fn_ = nullptr; }
            ~scope_guard() { if (fn_) fn_(); }
            scope_guard(const scope_guard &) = delete;
            void operator=(const scope_guard &) = delete;
        private:
            std::function<void()> fn_;
    };
}