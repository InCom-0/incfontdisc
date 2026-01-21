#if defined(INCFONTDISC_BACKEND_DWRITE)

#include <incfontdisc_private/backend.hpp>

#include <dwrite.h>
#include <wrl/client.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>


namespace incfontdisc::detail {

thread_local std::vector<FontDescriptor> DWriteBackend::cached_fonts_{};
thread_local bool                      DWriteBackend::cache_valid_ = false;

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

std::string
to_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char ch : value) { lowered.push_back(static_cast<char>(std::tolower(ch))); }
    return lowered;
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
    if (cache_valid_) { return cached_fonts_; }
    auto refreshed = refresh_fonts();
    if (!refreshed) {
        return std::unexpected(refreshed.error());
    }
    return cached_fonts_;
}

std::expected<void, Error>
DWriteBackend::refresh_fonts() {
    auto enumerated = enumerate_fonts();
    if (!enumerated) {
        return std::unexpected(enumerated.error());
    }
    cached_fonts_ = *enumerated;
    cache_valid_  = true;
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

std::expected<std::vector<FontDescriptor>, Error>
DWriteBackend::match_fonts(const FontQuery &query) {
    return list_fonts().and_then([&](std::vector<FontDescriptor> fonts) {
        if (! fonts.empty()) {
            std::vector<FontDescriptor> matches;
            matches.reserve(fonts.size());
            for (const auto &font : fonts) {
                if (matches_query(font, query)) { matches.push_back(font); }
            }
            return std::expected<std::vector<FontDescriptor>, Error>{std::move(matches)};
        }
        return std::expected<std::vector<FontDescriptor>, Error>{std::move(fonts)};
    });
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