#pragma once

#include <incfontdisc/incfontdisc.hpp>

namespace incfontdisc::detail {

#if defined(INCFONTDISC_BACKEND_FONTCONFIG)

class FontconfigBackend final {
public:
    std::expected<std::vector<FontDescriptor>, Error>
    list_fonts();
    std::expected<void, Error>
    refresh_fonts();
    std::expected<FontMatch, Error>
    match_fonts(const FontQuery &query);
    std::expected<ByteBuffer, Error>
    load_font_data(const FontId &id);

private:
    std::expected<std::vector<FontDescriptor>, Error>
    enumerate_fonts();
};

using Backend = FontconfigBackend;

#elif defined(INCFONTDISC_BACKEND_DWRITE)

class DWriteBackend final {
public:
    std::expected<std::vector<FontDescriptor>, Error>
    list_fonts();
    std::expected<void, Error>
    refresh_fonts();
    std::expected<FontMatch, Error>
    match_fonts(const FontQuery &query);
    std::expected<ByteBuffer, Error>
    load_font_data(const FontId &id);

private:
    std::expected<std::vector<FontDescriptor>, Error>
    enumerate_fonts();
};

using Backend = DWriteBackend;

#else

class BackendUnavailable final {
public:
    std::expected<std::vector<FontDescriptor>, Error>
    list_fonts() {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "No backend configured"});
    }
    std::expected<void, Error>
    refresh_fonts() {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "No backend configured"});
    }
    std::expected<FontMatch, Error>
    match_fonts(const FontQuery &) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "No backend configured"});
    }
    std::expected<ByteBuffer, Error>
    load_font_data(const FontId &) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "No backend configured"});
    }
};

using Backend = BackendUnavailable;

#endif

inline Backend &
backend_instance() {
    static Backend backend{};
    return backend;
}

} // namespace incfontdisc::detail