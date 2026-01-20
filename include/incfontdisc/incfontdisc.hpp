#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace incfontdisc {

enum class ErrorCode {
	BackendUnavailable,
	NotImplemented,
	InvalidArgument,
	SystemError
};

struct Error {
	ErrorCode code{};
	std::string message{};
};

struct FontId {
	std::string value{};
};

struct FontDescriptor {
	FontId id{};
	std::string family{};
	std::string style{};
	int weight = 400;
	int stretch = 100;
	bool italic = false;
};

struct FontQuery {
	std::optional<std::string> family{};
	std::optional<std::string> style{};
	std::optional<int> weight{};
	std::optional<int> stretch{};
	std::optional<bool> italic{};
};

using ByteBuffer = std::vector<std::byte>;

std::expected<std::vector<FontDescriptor>, Error> list_fonts();
std::expected<std::vector<FontDescriptor>, Error> match_fonts(const FontQuery &query);
std::expected<ByteBuffer, Error> load_font_data(const FontId &id);

} // namespace incfontdisc