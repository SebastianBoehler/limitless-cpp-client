#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace limitless {

// Quote integers with more than 15 digits so token ids and salts stay exact.
std::string quote_big_integers(std::string_view json_text);
nlohmann::json parse_json(std::string_view json_text);

}  // namespace limitless
