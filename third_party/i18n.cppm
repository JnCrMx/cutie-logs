module;

#include "fake_libintl.hpp"
#include <cstring>
#define throw (void) // very bad hack to prevent -fno-exceptions from breaking the code
#include <i18n/simple.hpp>

export module i18n;

export namespace mfk::i18n {
    using mfk::i18n::CompileTimeString;

    inline namespace literals {
        using mfk::i18n::literals::operator""_;
    }
}
