//===------------------------- cxa_default_handlers.cpp -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// This file implements the default terminate_handler and unexpected_handler.
//===----------------------------------------------------------------------===//

#include <stdexcept>
#include <new>
#include <exception>
#include "abort_message.h"
#include "config.h" // For __sync_swap
#include "cxxabi.h"
#include "cxa_handlers.hpp"
#include "cxa_exception.hpp"
#include "private_typeinfo.h"

static const char* cause = "uncaught";

#ifdef __k1om__
#define __sync_swap(a, b) __sync_lock_test_and_set(a, b)
#else
#define __sync_swap(a, b) __atomic_exchange_n(a, b, __ATOMIC_SEQ_CST)
#endif

__attribute__((noreturn))
static void default_terminate_handler()
{
    // If there might be an uncaught exception
    using namespace __cxxabiv1;
    __cxa_eh_globals* globals = __cxa_get_globals_fast();
    if (globals)
    {
        __cxa_exception* exception_header = globals->caughtExceptions;
        // If there is an uncaught exception
        if (exception_header)
        {
            _Unwind_Exception* unwind_exception =
                reinterpret_cast<_Unwind_Exception*>(exception_header + 1) - 1;
            bool native_exception =
                (unwind_exception->exception_class   & get_vendor_and_language) ==
                                 (kOurExceptionClass & get_vendor_and_language);
            if (native_exception)
            {
                void* thrown_object =
                    unwind_exception->exception_class == kOurDependentExceptionClass ?
                        ((__cxa_dependent_exception*)exception_header)->primaryException :
                        exception_header + 1;
                const __shim_type_info* thrown_type =
                    static_cast<const __shim_type_info*>(exception_header->exceptionType);
                // Try to get demangled name of thrown_type
                int status;
                char buf[1024];
                size_t len = sizeof(buf);
                const char* name = __cxa_demangle(thrown_type->name(), buf, &len, &status);
                if (status != 0)
                    name = thrown_type->name();
                // If the uncaught exception can be caught with std::exception&
                const __shim_type_info* catch_type =
				 static_cast<const __shim_type_info*>(&typeid(std::exception));
                if (catch_type->can_catch(thrown_type, thrown_object))
                {
                    // Include the what() message from the exception
                    const std::exception* e = static_cast<const std::exception*>(thrown_object);
                    abort_message("terminating with %s exception of type %s: %s",
                                  cause, name, e->what());
                }
                else
                    // Else just note that we're terminating with an exception
                    abort_message("terminating with %s exception of type %s",
                                   cause, name);
            }
            else
                // Else we're terminating with a foreign exception
                abort_message("terminating with %s foreign exception", cause);
        }
    }
    // Else just note that we're terminating
    abort_message("terminating");
}

__attribute__((noreturn))
static void default_unexpected_handler()
{
    cause = "unexpected";
    std::terminate();
}


//
// Global variables that hold the pointers to the current handler
//
std::terminate_handler  __cxa_terminate_handler = default_terminate_handler;
std::unexpected_handler __cxa_unexpected_handler = default_unexpected_handler;

// In the future these will become:
// std::atomic<std::terminate_handler>  __cxa_terminate_handler(default_terminate_handler);
// std::atomic<std::unexpected_handler> __cxa_unexpected_handler(default_unexpected_handler);

namespace std
{

unexpected_handler
set_unexpected(unexpected_handler func) _NOEXCEPT
{
	if (func == 0)
		func = default_unexpected_handler;
	return __sync_swap(&__cxa_unexpected_handler, func);
//  Using of C++11 atomics this should be rewritten
//  return __cxa_unexpected_handler.exchange(func, memory_order_acq_rel);
}

terminate_handler
set_terminate(terminate_handler func) _NOEXCEPT
{
	if (func == 0)
		func = default_terminate_handler;
	return __sync_swap(&__cxa_terminate_handler, func);
//  Using of C++11 atomics this should be rewritten
//  return __cxa_terminate_handler.exchange(func, memory_order_acq_rel);
}

}
