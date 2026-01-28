#if defined(INCFONTDISC_BACKEND_FONTCONFIG)

#include <incfontdisc_private/backend.hpp>

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace incfontdisc::detail {

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

std::string
normalize_family(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

int
levenshtein_distance(const std::string &a, const std::string &b) {
    if (a == b) { return 0; }
    if (a.empty()) { return static_cast<int>(b.size()); }
    if (b.empty()) { return static_cast<int>(a.size()); }

    std::vector<int> prev(b.size() + 1, 0);
    std::vector<int> curr(b.size() + 1, 0);
    for (size_t j = 0; j <= b.size(); ++j) { prev[j] = static_cast<int>(j); }

    for (size_t i = 1; i <= a.size(); ++i) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,
                curr[j - 1] + 1,
                prev[j - 1] + cost
            });
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}

float
family_similarity(std::string_view candidate, std::string_view query) {
    const auto norm_candidate = normalize_family(candidate);
    const auto norm_query     = normalize_family(query);
    if (norm_candidate.empty() || norm_query.empty()) { return 0.0f; }
    if (norm_candidate == norm_query) { return 1.0f; }
    const int max_len = static_cast<int>(std::max(norm_candidate.size(), norm_query.size()));
    const int dist    = levenshtein_distance(norm_candidate, norm_query);
    const float base  = 1.0f - std::min(static_cast<float>(dist) / static_cast<float>(max_len), 1.0f);
    return std::max(0.0f, base);
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

float
face_score(const FontDescriptor &font, const FontQuery &query) {
    float total = 0.0f;
    int   count = 0;

    if (query.style) {
        ++count;
        total += (to_lower(font.style) == to_lower(*query.style)) ? 1.0f : 0.0f;
    }
    if (query.weight) {
        ++count;
        const float diff  = std::abs(static_cast<float>(font.weight - *query.weight));
        const float score = 1.0f - std::min(diff / 900.0f, 1.0f);
        total += score;
    }
    if (query.stretch) {
        ++count;
        const float range = (font.stretch <= 9 && *query.stretch <= 9) ? 8.0f : 150.0f;
        const float diff  = std::abs(static_cast<float>(font.stretch - *query.stretch));
        const float score = 1.0f - std::min(diff / range, 1.0f);
        total += score;
    }
    if (query.italic) {
        ++count;
        total += (font.italic == *query.italic) ? 1.0f : 0.0f;
    }

    if (count == 0) {
        return 0.0f;
    }
    return total / static_cast<float>(count);
}

std::optional<FontDescriptor>
descriptor_from_pattern(FcPattern *font) {
    if (!font) {
        return std::nullopt;
    }

    FcChar8 *family = nullptr;
    FcChar8 *style  = nullptr;
    FcChar8 *file   = nullptr;
    int      weight = 400;
    int      width  = 100;
    int      slant  = 0;
    int      index  = 0;

    if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) {
        return std::nullopt;
    }
    FcPatternGetString(font, FC_STYLE, 0, &style);
    if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
        return std::nullopt;
    }
    FcPatternGetInteger(font, FC_WEIGHT, 0, &weight);
    FcPatternGetInteger(font, FC_WIDTH, 0, &width);
    FcPatternGetInteger(font, FC_SLANT, 0, &slant);
    FcPatternGetInteger(font, FC_INDEX, 0, &index);

    FontDescriptor descriptor{};
    descriptor.family  = reinterpret_cast<const char *>(family);
    if (style) {
        descriptor.style = reinterpret_cast<const char *>(style);
    }
    descriptor.weight  = weight;
    descriptor.stretch = width;
    descriptor.italic  = (slant == FC_SLANT_ITALIC || slant == FC_SLANT_OBLIQUE);
    descriptor.id.value = std::string(reinterpret_cast<const char *>(file)) + "#" + std::to_string(index);

    return descriptor;
}

std::expected<ByteBuffer, Error>
read_file_bytes(const std::string &path) {
    std::error_code ec;
    const auto      fs_path = std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t *>(path.data()),
        reinterpret_cast<const char8_t *>(path.data() + path.size())));
    if (!std::filesystem::exists(fs_path, ec)) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "Font file does not exist"});
    }

    std::ifstream stream(fs_path, std::ios::binary);
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
    return enumerate_fonts();
}

std::expected<void, Error>
FontconfigBackend::refresh_fonts() {
    auto enumerated = enumerate_fonts();
    if (!enumerated) {
        return std::unexpected(enumerated.error());
    }
    return {};
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
        auto        descriptor = descriptor_from_pattern(font);
        if (!descriptor) {
            continue;
        }

        fonts.push_back(std::move(*descriptor));
    }

    FcFontSetDestroy(font_set);
    return fonts;
}

std::expected<FontMatch, Error>
FontconfigBackend::match_fonts(FontQuery query) {
    if (FcInit() == FcFalse) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "fontconfig failed to initialize"});
    }
    if (!query.family) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "FontQuery.family must be set"});
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

    const auto query_family_lower = to_lower(*query.family);
    std::string best_family_norm;
    float best_family_score = 0.0f;

    for (int i = 0; i < font_set->nfont; ++i) {
        FcPattern *font   = font_set->fonts[i];
        FcChar8   *family = nullptr;
        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) {
            continue;
        }
        const std::string family_name = reinterpret_cast<const char *>(family);
        if (family_name.empty()) {
            continue;
        }

        const auto family_lower = to_lower(family_name);
        const auto family_norm  = normalize_family(family_name);
        if (family_lower == query_family_lower) {
            best_family_norm  = family_norm;
            best_family_score = 1.0f;
            break;
        }

        const float score = family_similarity(family_name, *query.family);
        if (score > best_family_score) {
            best_family_score = score;
            best_family_norm  = family_norm;
        }
    }

    if (best_family_norm.empty()) {
        FcFontSetDestroy(font_set);
        return std::unexpected(
            Error{ErrorCode::NoFontsFound, "No fonts found on the system, this should be impossible."});
    }

    std::vector<FcPattern *> best_family_fonts;
    best_family_fonts.reserve(static_cast<size_t>(font_set->nfont));
    for (int i = 0; i < font_set->nfont; ++i) {
        FcPattern *font   = font_set->fonts[i];
        FcChar8   *family = nullptr;
        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) {
            continue;
        }
        const std::string family_name = reinterpret_cast<const char *>(family);
        if (family_name.empty()) {
            continue;
        }
        if (normalize_family(family_name) != best_family_norm) {
            continue;
        }
        best_family_fonts.push_back(font);
    }

    if (!query.style) { query.style = "Regular"; }
    for (auto *font : best_family_fonts) {
        auto descriptor = descriptor_from_pattern(font);
        if (!descriptor) {
            continue;
        }

        bool exact = true;
        if (query.style && to_lower(descriptor->style) != to_lower(*query.style)) { exact = false; }
        if (query.weight && descriptor->weight != *query.weight) { exact = false; }
        if (query.stretch && descriptor->stretch != *query.stretch) { exact = false; }
        if (query.italic && descriptor->italic != *query.italic) { exact = false; }
        if (exact) {
            FcFontSetDestroy(font_set);
            return FontMatch{.font = std::move(*descriptor), .family_score = best_family_score, .face_score = 1.0f};
        }
    }

    if (!query.weight) { query.weight = 400; }
    if (!query.stretch) { query.stretch = 100; }
    if (!query.italic) { query.italic = false; }

    FontMatch res_match{.family_score = best_family_score, .face_score = 0.0f};
    for (auto *font : best_family_fonts) {
        auto descriptor = descriptor_from_pattern(font);
        if (!descriptor) {
            continue;
        }

        const float score = face_score(*descriptor, query);
        if (score > res_match.face_score) {
            res_match.font       = std::move(*descriptor);
            res_match.face_score = score;
        }
    }

    FcFontSetDestroy(font_set);
    return res_match;
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