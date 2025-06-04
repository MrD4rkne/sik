#include <cstring>
#include <sstream>
#include <string>

#include "input.h"

namespace input {
args_parses_t::args_parses_t(
    int argc, char* argv[],
    const std::unordered_map<std::string, arg_t>& allowed_args)
    : value_args(), flags(), allowed_args(allowed_args) {
    for (int i = 1; i < argc; i++) {
        std::string current_arg = argv[i];

        auto it = allowed_args.find(current_arg);
        // Check if it's a recognized argument
        if (it == allowed_args.end()) {
            throw std::invalid_argument("Unknown argument: " + current_arg);
        }

        // Check if this flag already has a value
        if (flags.find(current_arg) != flags.end() ||
            value_args.find(current_arg) != value_args.end()) {
            throw std::invalid_argument("Duplicate argument: " + current_arg);
        }

        bool is_flag = it->second.is_flag;
        if (!is_flag && (i + 1 >= argc || argv[i + 1][0] == '-')) {
            throw std::invalid_argument("Argument requires a value: " +
                                        current_arg);
        }

        if (is_flag) {
            flags.insert(current_arg);
        } else {
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                throw std::invalid_argument("Argument requires a value: " +
                                            current_arg);
            }

            value_args[current_arg] = argv[i + 1];
            i++; // Skip the next argument as it's the value
        }
    }

    for (const auto& [key, opt] : allowed_args) {
        auto& [is_flag, necessary, has_default, default_value] = opt;
        if (is_flag) {
            if (flags.find(key) == flags.end() && necessary) {
                throw std::invalid_argument("Missing required flag: " + key);
            }
        } else {
            if (value_args.find(key) == value_args.end()) {
                if (necessary) {
                    throw std::invalid_argument("Missing required argument: " +
                                                key);
                }
            }
        }
    }
}

const std::string& args_parses_t::get_value(const std::string& arg) const {
    auto it = value_args.find(arg);
    if (it != value_args.end()) {
        return it->second;
    }

    auto it2 = allowed_args.find(arg);
    if (it2 != allowed_args.end() && it2->second.has_default_value) {
        return it2->second.default_value;
    }

    throw std::invalid_argument("Argument not found: " + arg);
}

bool args_parses_t::has_arg(const std::string& arg) const {
    auto it = value_args.find(arg);
    if (it != value_args.end()) {
        return true;
    }

    auto it2 = allowed_args.find(arg);
    if (it2 != allowed_args.end() && it2->second.has_default_value) {
        return true;
    }

    return false;
}

bool args_parses_t::has_flag(const std::string& flag) const {
    return flags.find(flag) != flags.end();
}

std::string args_parses_t::to_string() const {
    std::stringstream ss;
    for (const auto& flag : flags) {
        ss << flag << "\n";
    }

    for (const auto& [key, value] : value_args) {
        ss << key << ": " << value << "\n";
    }

    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const input::args_parses_t& args) {
    os << args.to_string();
    return os;
}
} // namespace input