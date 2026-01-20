#if defined(INCFONTDISC_BACKEND_FONTCONFIG)

#include <incfontdisc_private/backend.hpp>

#include <fontconfig/fontconfig.h>

namespace incfontdisc::detail {

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::list_fonts() {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "fontconfig enumeration not implemented yet"});
}

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::match_fonts(const FontQuery &) {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "fontconfig matching not implemented yet"});
}

std::expected<ByteBuffer, Error>
FontconfigBackend::load_font_data(const FontId &) {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "fontconfig loading not implemented yet"});
}

} // namespace incfontdisc::detail

#endif