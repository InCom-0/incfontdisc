#include <incfontdisc/incfontdisc.hpp>
#include <incfontdisc_private/backend.hpp>

namespace incfontdisc {

std::expected<std::vector<FontDescriptor>, Error>
list_fonts() {
    return detail::backend_instance().list_fonts();
}

std::expected<std::vector<FontDescriptor>, Error>
refresh_fonts() {
    return detail::backend_instance().refresh_fonts();
}

std::expected<std::vector<FontDescriptor>, Error>
match_fonts(const FontQuery &query) {
    return detail::backend_instance().match_fonts(query);
}

std::expected<ByteBuffer, Error>
load_font_data(const FontId &id) {
    return detail::backend_instance().load_font_data(id);
}

} // namespace incfontdisc
