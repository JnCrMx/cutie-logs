#pragma once
#define _LIBINTL_H

namespace intl {
    const char* dgettext(const char* domainname, const char* msgid);
}

#ifdef __has_attribute
#if __has_attribute(mfk_i18n)
#define I18N_ATTR(x) [[mfk::i18n ## x]]
#else
#define I18N_ATTR(x)
#endif
#else
#define I18N_ATTR(x)
#endif

extern "C" {
    I18N_ATTR() char* dgettext(I18N_ATTR(_domain_begin) const char* domainname, I18N_ATTR(_singular_begin) const char* msgid) {
        const char* p = intl::dgettext(domainname, msgid);
        return p ? const_cast<char*>(p) : const_cast<char*>(msgid);
    }
}

#undef I18N_ATTR
