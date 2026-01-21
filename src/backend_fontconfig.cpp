#if defined(INCFONTDISC_BACKEND_FONTCONFIG)

#include <incfontdisc_private/backend.hpp>

#include <fontconfig/fontconfig.h>

#include <cctype>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace incfontdisc::detail {

thread_local std::vector<FontDescriptor> FontconfigBackend::cached_fonts_{};
thread_local bool                      FontconfigBackend::cache_valid_ = false;

namespace {

std::string
to_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lowered;
}

bool
matches_query(const FontDescriptor &font, const FontQuery &query) {
    if (query.family) {
        if (to_lower(font.family) != to_lower(*query.family)) {
            return false;
        }
    }
    if (query.style) {
        if (to_lower(font.style) != to_lower(*query.style)) {
            return false;
        }
    }
    if (query.weight && font.weight != *query.weight) {
        return false;
    }
    if (query.stretch && font.stretch != *query.stretch) {
        return false;
    }
    if (query.italic && font.italic != *query.italic) {
        return false;
    }
    return true;
}

std::expected<ByteBuffer, Error>
read_file_bytes(const std::string &path) {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(path), ec)) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "Font file does not exist"});
    }

    std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
    if (!stream) {
        return std::unexpected(Error{ErrorCode::SystemError, "Failed to open font file"});
    }

    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (size <= 0) {
        return std::unexpected(Error{ErrorCode::SystemError, "Font file is empty"});
    }

    ByteBuffer buffer(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char *>(buffer.data()), size);
    if (!stream) {
        return std::unexpected(Error{ErrorCode::SystemError, "Failed to read font file"});
    }
    return buffer;
}

std::pair<std::string, int>
parse_font_id(const FontId &id) {
    const auto hash_pos = id.value.rfind('#');
    if (hash_pos == std::string::npos) {
        return {id.value, 0};
    }
    std::string path = id.value.substr(0, hash_pos);
    int index = 0;
    try {
        index = std::stoi(id.value.substr(hash_pos + 1));
    } catch (...) {
        index = 0;
    }
    return {std::move(path), index};
}

} // namespace

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::list_fonts() {
    if (cache_valid_) {
        return cached_fonts_;
    }
    return refresh_fonts();
}

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::refresh_fonts() {
    auto enumerated = enumerate_fonts();
    if (enumerated) {
        cached_fonts_ = *enumerated;
        cache_valid_  = true;
    }
    return enumerated;
}

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::enumerate_fonts() {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }

    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        return std::unexpected(Error{ErrorCode::SystemError, "fontconfig pattern creation failed"});
    }

    FcObjectSet *object_set = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_WEIGHT, FC_WIDTH, FC_SLANT, FC_FILE,
                                               FC_INDEX, nullptr);
    if (!object_set) {
        FcPatternDestroy(pattern);
        return std::unexpected(Error{ErrorCode::SystemError, "fontconfig object set creation failed"});
    }

    FcFontSet *font_set = FcFontList(nullptr, pattern, object_set);
    FcObjectSetDestroy(object_set);
    FcPatternDestroy(pattern);

    if (!font_set) {
        return std::unexpected(Error{ErrorCode::SystemError, "fontconfig font listing failed"});
    }

    std::vector<FontDescriptor> fonts;
    fonts.reserve(static_cast<size_t>(font_set->nfont));

    for (int i = 0; i < font_set->nfont; ++i) {
        FcPattern *font = font_set->fonts[i];

        FcChar8 *family = nullptr;
        FcChar8 *style = nullptr;
        FcChar8 *file = nullptr;
        int weight = 400;
        int width = 100;
        int slant = 0;
        int index = 0;

        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) {
            continue;
        }
        FcPatternGetString(font, FC_STYLE, 0, &style);
        if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
            continue;
        }
        FcPatternGetInteger(font, FC_WEIGHT, 0, &weight);
        FcPatternGetInteger(font, FC_WIDTH, 0, &width);
        FcPatternGetInteger(font, FC_SLANT, 0, &slant);
        FcPatternGetInteger(font, FC_INDEX, 0, &index);

        FontDescriptor descriptor{};
        descriptor.family = reinterpret_cast<const char *>(family);
        if (style) {
            descriptor.style = reinterpret_cast<const char *>(style);
        }
        descriptor.weight = weight;
        descriptor.stretch = width;
        descriptor.italic = (slant == FC_SLANT_ITALIC || slant == FC_SLANT_OBLIQUE);
        descriptor.id.value = std::string(reinterpret_cast<const char *>(file)) + "#" + std::to_string(index);

        fonts.push_back(std::move(descriptor));
    }

    FcFontSetDestroy(font_set);
    return fonts;
}

std::expected<std::vector<FontDescriptor>, Error>
FontconfigBackend::match_fonts(const FontQuery &query) {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }
    return list_fonts().and_then([&](std::vector<FontDescriptor> fonts) {
        if (!fonts.empty()) {
            std::vector<FontDescriptor> matches;
            matches.reserve(fonts.size());
            for (const auto &font : fonts) {
                if (matches_query(font, query)) {
                    matches.push_back(font);
                }
            }
            return std::expected<std::vector<FontDescriptor>, Error>{std::move(matches)};
        }
        return std::expected<std::vector<FontDescriptor>, Error>{std::move(fonts)};
    });
}

std::expected<ByteBuffer, Error>
FontconfigBackend::load_font_data(const FontId &id) {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }

    const auto [path, index] = parse_font_id(id);
    (void)index;
    if (path.empty()) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "FontId is empty"});
    }
    return read_file_bytes(path);
}

} // namespace incfontdisc::detail

#endif