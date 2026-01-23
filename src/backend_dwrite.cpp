#if defined(INCFONTDISC_BACKEND_DWRITE)

#include <incfontdisc_private/backend.hpp>

#include <dwrite_1.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>


namespace incfontdisc::detail {
namespace {

Microsoft::WRL::ComPtr<IDWriteFactory>
get_factory() {
    static Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    if (factory) { return factory; }
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    if (FAILED(hr)) { factory.Reset(); }
    return factory;
}

Microsoft::WRL::ComPtr<IDWriteFactory1>
get_factory1() {
    static Microsoft::WRL::ComPtr<IDWriteFactory1> factory1;
    if (factory1) { return factory1; }

    auto base_factory = get_factory();
    if (base_factory && SUCCEEDED(base_factory.As(&factory1)) && factory1) { return factory1; }

    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1),
                                     reinterpret_cast<IUnknown **>(factory1.GetAddressOf()));
    if (FAILED(hr)) { factory1.Reset(); }
    return factory1;
}

std::string
to_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char ch : value) { lowered.push_back(static_cast<char>(std::tolower(ch))); }
    return lowered;
}

std::string
normalize_family(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) { normalized.push_back(static_cast<char>(std::tolower(ch))); }
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
            curr[j]        = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
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
    const int   max_len = static_cast<int>(std::max(norm_candidate.size(), norm_query.size()));
    const int   dist    = levenshtein_distance(norm_candidate, norm_query);
    const float base    = 1.0f - std::min(static_cast<float>(dist) / static_cast<float>(max_len), 1.0f);
    return std::max(0.0f, base);
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
        const float diff   = std::abs(static_cast<float>(font.weight - *query.weight));
        const float score  = 1.0f - std::min(diff / 900.0f, 1.0f);
        total             += score;
    }
    if (query.stretch) {
        ++count;
        const float range  = (font.stretch <= 9 && *query.stretch <= 9) ? 8.0f : 150.0f;
        const float diff   = std::abs(static_cast<float>(font.stretch - *query.stretch));
        const float score  = 1.0f - std::min(diff / range, 1.0f);
        total             += score;
    }
    if (query.italic) {
        ++count;
        total += (font.italic == *query.italic) ? 1.0f : 0.0f;
    }

    if (count == 0) { return 0.0f; }
    return total / static_cast<float>(count);
}

bool
matches_query(const FontDescriptor &font, const FontQuery &query) {
    if (query.family) {
        if (to_lower(font.family) != to_lower(*query.family)) { return false; }
    }
    if (query.style) {
        if (to_lower(font.style) != to_lower(*query.style)) { return false; }
    }
    if (query.weight && font.weight != *query.weight) { return false; }
    if (query.stretch && font.stretch != *query.stretch) { return false; }
    if (query.italic && font.italic != *query.italic) { return false; }
    return true;
}

std::string
utf8_from_wide(const std::wstring &value) {
    if (value.empty()) { return {}; }
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) { return {}; }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr,
                        nullptr);
    return result;
}

std::wstring
wide_from_utf8(const std::string &value) {
    if (value.empty()) { return {}; }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) { return {}; }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::filesystem::path
path_from_utf8(const std::string &value) {
    if (value.empty()) { return {}; }
    const auto   *data = reinterpret_cast<const char8_t *>(value.data());
    std::u8string u8value(data, data + value.size());
    return std::filesystem::path(u8value);
}

std::wstring
get_localized_string(IDWriteLocalizedStrings *strings) {
    if (! strings) { return {}; }
    UINT32 index  = 0;
    BOOL   exists = FALSE;
    if (SUCCEEDED(strings->FindLocaleName(L"en-us", &index, &exists)) && exists) {
        // use index as found
    }
    else { index = 0; }
    UINT32 length = 0;
    if (FAILED(strings->GetStringLength(index, &length))) { return {}; }
    std::wstring result(length + 1, L'\0');
    if (FAILED(strings->GetString(index, result.data(), length + 1))) { return {}; }
    result.resize(length);
    return result;
}

std::optional<DWRITE_FONT_STYLE>
style_from_query(const FontQuery &query) {
    if (query.italic) { return *query.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL; }
    if (! query.style) { return std::nullopt; }

    const auto lowered = to_lower(*query.style);
    if (lowered == "italic") { return DWRITE_FONT_STYLE_ITALIC; }
    if (lowered == "oblique") { return DWRITE_FONT_STYLE_OBLIQUE; }
    if (lowered == "regular" || lowered == "normal") { return DWRITE_FONT_STYLE_NORMAL; }
    return std::nullopt;
}

std::wstring
font_file_path(IDWriteFactory *factory, IDWriteFontFile *file);

std::optional<FontDescriptor>
descriptor_from_font(IDWriteFactory *factory, IDWriteFont *font, const std::string &family_utf8) {
    if (! factory || ! font) { return std::nullopt; }

    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> face_names;
    std::wstring                                    style_name;
    if (SUCCEEDED(font->GetFaceNames(&face_names)) && face_names) {
        style_name = get_localized_string(face_names.Get());
    }

    Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
    if (FAILED(font->CreateFontFace(&font_face)) || ! font_face) { return std::nullopt; }

    UINT32 file_count = 0;
    if (FAILED(font_face->GetFiles(&file_count, nullptr)) || file_count == 0) { return std::nullopt; }

    std::vector<Microsoft::WRL::ComPtr<IDWriteFontFile>> files(file_count);
    std::vector<IDWriteFontFile *>                       raw_files(file_count, nullptr);
    if (FAILED(font_face->GetFiles(&file_count, raw_files.data()))) { return std::nullopt; }
    for (UINT32 k = 0; k < file_count; ++k) { files[k].Attach(raw_files[k]); }

    std::wstring file_path_wide = font_file_path(factory, files.front().Get());
    if (file_path_wide.empty()) { return std::nullopt; }

    FontDescriptor descriptor{};
    descriptor.family  = family_utf8;
    descriptor.style   = utf8_from_wide(style_name);
    descriptor.weight  = static_cast<int>(font->GetWeight());
    descriptor.stretch = static_cast<int>(font->GetStretch());
    const auto style   = font->GetStyle();
    descriptor.italic  = (style == DWRITE_FONT_STYLE_ITALIC || style == DWRITE_FONT_STYLE_OBLIQUE);

    const UINT32      face_index = font_face->GetIndex();
    const std::string file_utf8  = utf8_from_wide(file_path_wide);
    descriptor.id.value          = file_utf8 + "#" + std::to_string(face_index);

    return descriptor;
}

std::wstring
font_file_path(IDWriteFactory *factory, IDWriteFontFile *file) {
    if (! factory || ! file) { return {}; }
    Microsoft::WRL::ComPtr<IDWriteFontFileLoader> loader;
    if (FAILED(file->GetLoader(&loader)) || ! loader) { return {}; }

    Microsoft::WRL::ComPtr<IDWriteLocalFontFileLoader> local_loader;
    if (FAILED(loader.As(&local_loader)) || ! local_loader) { return {}; }

    const void *key      = nullptr;
    UINT32      key_size = 0;
    if (FAILED(file->GetReferenceKey(&key, &key_size))) { return {}; }

    UINT32 path_length = 0;
    if (FAILED(local_loader->GetFilePathLengthFromKey(key, key_size, &path_length))) { return {}; }

    std::wstring path(path_length + 1, L'\0');
    if (FAILED(local_loader->GetFilePathFromKey(key, key_size, path.data(), path_length + 1))) { return {}; }
    path.resize(path_length);
    return path;
}

std::expected<ByteBuffer, Error>
read_file_bytes(const std::string &path) {
    std::error_code ec;
    const auto      fs_path = path_from_utf8(path);
    if (! std::filesystem::exists(fs_path, ec)) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "Font file does not exist"});
    }

    std::ifstream stream(fs_path, std::ios::binary);
    if (! stream) { return std::unexpected(Error{ErrorCode::SystemError, "Failed to open font file"}); }

    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (size <= 0) { return std::unexpected(Error{ErrorCode::SystemError, "Font file is empty"}); }

    ByteBuffer buffer(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char *>(buffer.data()), size);
    if (! stream) { return std::unexpected(Error{ErrorCode::SystemError, "Failed to read font file"}); }
    return buffer;
}

std::pair<std::string, int>
parse_font_id(const FontId &id) {
    const auto hash_pos = id.value.rfind('#');
    if (hash_pos == std::string::npos) { return {id.value, 0}; }
    std::string path  = id.value.substr(0, hash_pos);
    int         index = 0;
    try {
        index = std::stoi(id.value.substr(hash_pos + 1));
    }
    catch (...) {
        index = 0;
    }
    return {std::move(path), index};
}

} // namespace

std::expected<std::vector<FontDescriptor>, Error>
DWriteBackend::list_fonts() {
    return enumerate_fonts();
}

std::expected<void, Error>
DWriteBackend::refresh_fonts() {
    auto enumerated = enumerate_fonts();
    if (! enumerated) { return std::unexpected(enumerated.error()); }
    return {};
}

std::expected<std::vector<FontDescriptor>, Error>
DWriteBackend::enumerate_fonts() {
    auto factory = get_factory();
    if (! factory) { return std::unexpected(Error{ErrorCode::BackendUnavailable, "DirectWrite factory unavailable"}); }

    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    HRESULT                                       hr = factory->GetSystemFontCollection(&collection);
    if (FAILED(hr) || ! collection) {
        return std::unexpected(Error{ErrorCode::SystemError, "DirectWrite font collection unavailable"});
    }

    const UINT32                family_count = collection->GetFontFamilyCount();
    std::vector<FontDescriptor> fonts;

    for (UINT32 i = 0; i < family_count; ++i) {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
        if (FAILED(collection->GetFontFamily(i, &family)) || ! family) { continue; }

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
        std::wstring                                    family_name;
        if (SUCCEEDED(family->GetFamilyNames(&family_names)) && family_names) {
            family_name = get_localized_string(family_names.Get());
        }
        const std::string family_utf8 = utf8_from_wide(family_name);

        const UINT32 font_count = family->GetFontCount();
        for (UINT32 j = 0; j < font_count; ++j) {
            Microsoft::WRL::ComPtr<IDWriteFont> font;
            if (FAILED(family->GetFont(j, &font)) || ! font) { continue; }

            Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> face_names;
            std::wstring                                    style_name;
            if (SUCCEEDED(font->GetFaceNames(&face_names)) && face_names) {
                style_name = get_localized_string(face_names.Get());
            }

            Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
            if (FAILED(font->CreateFontFace(&font_face)) || ! font_face) { continue; }

            UINT32 file_count = 0;
            if (FAILED(font_face->GetFiles(&file_count, nullptr)) || file_count == 0) { continue; }

            std::vector<Microsoft::WRL::ComPtr<IDWriteFontFile>> files(file_count);
            std::vector<IDWriteFontFile *>                       raw_files(file_count, nullptr);
            if (FAILED(font_face->GetFiles(&file_count, raw_files.data()))) { continue; }
            for (UINT32 k = 0; k < file_count; ++k) { files[k].Attach(raw_files[k]); }

            std::wstring file_path_wide = font_file_path(factory.Get(), files.front().Get());
            if (file_path_wide.empty()) { continue; }

            FontDescriptor descriptor{};
            descriptor.family  = family_utf8;
            descriptor.style   = utf8_from_wide(style_name);
            descriptor.weight  = static_cast<int>(font->GetWeight());
            descriptor.stretch = static_cast<int>(font->GetStretch());
            const auto style   = font->GetStyle();
            descriptor.italic  = (style == DWRITE_FONT_STYLE_ITALIC || style == DWRITE_FONT_STYLE_OBLIQUE);

            const UINT32      face_index = font_face->GetIndex();
            const std::string file_utf8  = utf8_from_wide(file_path_wide);
            descriptor.id.value          = file_utf8 + "#" + std::to_string(face_index);

            fonts.push_back(std::move(descriptor));
        }
    }

    return fonts;
}

std::expected<FontMatch, Error>
DWriteBackend::match_fonts(FontQuery query) {
    if (! query.family) { return std::unexpected(Error{ErrorCode::InvalidArgument, "FontQuery.family must be set"}); }

    auto factory = get_factory();
    if (! factory) { return std::unexpected(Error{ErrorCode::BackendUnavailable, "DirectWrite factory unavailable"}); }

    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    HRESULT                                       hr = factory->GetSystemFontCollection(&collection);
    if (FAILED(hr) || ! collection) {
        return std::unexpected(Error{ErrorCode::SystemError, "DirectWrite font collection unavailable"});
    }

    const auto                                query_family_lower = to_lower(*query.family);
    std::string                               best_family_norm;
    float                                     best_family_score = 0.0f;
    std::string                               best_family_name;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> best_family;

    const UINT32 family_count = collection->GetFontFamilyCount();
    for (UINT32 i = 0; i < family_count; ++i) {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
        if (FAILED(collection->GetFontFamily(i, &family)) || ! family) { continue; }

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
        std::wstring                                    family_name;
        if (SUCCEEDED(family->GetFamilyNames(&family_names)) && family_names) {
            family_name = get_localized_string(family_names.Get());
        }
        const std::string family_utf8 = utf8_from_wide(family_name);
        if (family_utf8.empty()) { continue; }

        const auto family_lower = to_lower(family_utf8);
        const auto family_norm  = normalize_family(family_utf8);

        if (family_lower == query_family_lower) {
            best_family_norm  = family_norm;
            best_family_score = 1.0f;
            best_family_name  = family_utf8;
            best_family       = family;
            break;
        }

        const float score = family_similarity(family_utf8, *query.family);
        if (score > best_family_score) {
            best_family_score = score;
            best_family_norm  = family_norm;
            best_family_name  = family_utf8;
            best_family       = family;
        }
    }

    if (best_family_norm.empty()) {
        return std::unexpected(
            Error{ErrorCode::NoFontsFound, "No fonts found on the system, this should be impossible."});
    }

    if (! query.style) { query.style = "Regular"; }
    if (best_family) {
        const UINT32 font_count = best_family->GetFontCount();
        for (UINT32 j = 0; j < font_count; ++j) {
            Microsoft::WRL::ComPtr<IDWriteFont> font;
            if (FAILED(best_family->GetFont(j, &font)) || ! font) { continue; }

            auto descriptor = descriptor_from_font(factory.Get(), font.Get(), best_family_name);
            if (! descriptor) { continue; }

            bool exact = true;
            if (query.style && to_lower(descriptor->style) != to_lower(*query.style)) { exact = false; }
            if (query.weight && descriptor->weight != *query.weight) { exact = false; }
            if (query.stretch && descriptor->stretch != *query.stretch) { exact = false; }
            if (query.italic && descriptor->italic != *query.italic) { exact = false; }
            if (exact) {
                // If there is exact match just return it
                return FontMatch{.font = std::move(*descriptor), .family_score = best_family_score, .face_score = 1.0f};
            }
        }
    }


    if (! query.weight) { query.weight = 400; }
    if (! query.stretch) { query.stretch = 100; }
    if (! query.italic) { query.italic = false; }


    FontMatch res_match{.family_score = best_family_score, .face_score = 0.0f};
    if (best_family) {
        const UINT32 font_count = best_family->GetFontCount();
        for (UINT32 j = 0; j < font_count; ++j) {
            Microsoft::WRL::ComPtr<IDWriteFont> font;
            if (FAILED(best_family->GetFont(j, &font)) || ! font) { continue; }

            auto descriptor = descriptor_from_font(factory.Get(), font.Get(), best_family_name);
            if (! descriptor) { continue; }

            const float score = face_score(*descriptor, query);
            if (score > res_match.face_score) {
                res_match.font       = std::move(*descriptor);
                res_match.face_score = score;
            }
        }
    }
    return res_match;
}

std::expected<ByteBuffer, Error>
DWriteBackend::load_font_data(const FontId &id) {
    const auto [path, index] = parse_font_id(id);
    (void)index;
    if (path.empty()) { return std::unexpected(Error{ErrorCode::InvalidArgument, "FontId is empty"}); }
    return read_file_bytes(path);
}

} // namespace incfontdisc::detail

#endif