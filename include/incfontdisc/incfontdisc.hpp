#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>


#if defined(INCFONTDISC_SHARED)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(INCFONTDISC_EXPORTS)
#define INCFONTDISC_API __declspec(dllexport)
#else
#define INCFONTDISC_API __declspec(dllimport)
#endif
#else
#define INCFONTDISC_API __attribute__((visibility("default")))
#endif

#else
// Static library
#define INCFONTDISC_API
#endif


namespace incfontdisc {

enum class ErrorCode {
    BackendUnavailable,
    NotImplemented,
    InvalidArgument,
    NoFontsFound,
    SystemError
};

struct INCFONTDISC_API Error {
    ErrorCode   code{};
    std::string message{};
};

struct INCFONTDISC_API FontId {
    std::string value{};
};

struct INCFONTDISC_API FontDescriptor {
    FontId      id{};
    std::string family{};
    std::string style{};
    int         weight  = 400;
    int         stretch = 100;
    bool        italic  = false;
};

struct INCFONTDISC_API FontMatch {
    FontDescriptor font{};
    float          family_score = 0.0f;
    float          face_score   = 0.0f;
};

struct INCFONTDISC_API FontQuery {
    std::optional<std::string> family{};
    std::optional<std::string> style{};
    std::optional<int>         weight{};
    std::optional<int>         stretch{};
    std::optional<bool>        italic{};
};

using ByteBuffer = std::vector<std::byte>;

INCFONTDISC_API std::expected<std::vector<FontDescriptor>, Error>
                list_fonts();
INCFONTDISC_API std::expected<void, Error>
                refresh_fonts();
INCFONTDISC_API std::expected<FontMatch, Error>
                match_fonts(const FontQuery &query);
INCFONTDISC_API std::expected<ByteBuffer, Error>
                load_font_data(const FontId &id);

} // namespace incfontdisc