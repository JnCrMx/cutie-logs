#include <cstddef>

extern "C" void __wasm_call_ctors();

[[clang::export_name("_initialize")]]
void _initialize() {
    __wasm_call_ctors();
}

extern "C" {

void* __cxa_allocate_exception(size_t) { __builtin_trap(); return nullptr; }
void __cxa_throw(void *, void *, void (*) (void *)) { __builtin_trap(); }

void* __cxa_atexit(void (*f)(void*), void* p, void* d);
void* __cxa_thread_atexit(void (*f)(void*), void* p, void* d) { return __cxa_atexit(f, p, d); }

}
