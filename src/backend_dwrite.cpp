#if defined(INCFONTDISC_BACKEND_DWRITE)

#include <incfontdisc_private/backend.hpp>

#include <dwrite.h>
#include <wrl/client.h>

namespace incfontdisc::detail {

namespace {


bool
ensure_factory() {
    static Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    if (factory) { return true; }
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    return SUCCEEDED(hr);
}

} // namespace

std::expected<std::vector<FontDescriptor>, Error>
DWriteBackend::list_fonts() {
    if (! ensure_factory()) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "DirectWrite factory unavailable"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "DirectWrite enumeration not implemented yet"});
}

std::expected<std::vector<FontDescriptor>, Error>
DWriteBackend::match_fonts(const FontQuery &) {
    if (! ensure_factory()) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "DirectWrite factory unavailable"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "DirectWrite matching not implemented yet"});
}

std::expected<ByteBuffer, Error>
DWriteBackend::load_font_data(const FontId &) {
    if (! ensure_factory()) {
        return std::unexpected(Error{ErrorCode::BackendUnavailable, "DirectWrite factory unavailable"});
    }
    return std::unexpected(Error{ErrorCode::NotImplemented, "DirectWrite loading not implemented yet"});
}

} // namespace incfontdisc::detail

#endif