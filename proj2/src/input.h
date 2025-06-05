#ifndef INPUT_H
#define INPUT_H

#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

/// @brief Stuff related to input parsing and validation.
namespace input {

/// @brief Structure representing an argument or flag in command line arguments.
struct arg_t {
    /// @brief Creates a flag argument.
    /// @param name The name of the flag.
    /// @param necessary Whether the flag is necessary.
    /// @return A pair containing the flag name and its properties.
    static auto get_flag(const std::string& name, bool necessary = false) {
        return std::make_pair(name, arg_t(true, necessary));
    }

    /// @brief Creates a value argument.
    /// @param name The name of the argument.
    /// @param necessary Whether the argument is necessary.
    /// @param default_value The default value for the argument if it is not
    /// provided.
    /// @return A pair containing the argument name and its properties.
    static auto get_arg(const std::string& name, bool necessary = true,
                        const std::string& default_value = "") {
        return std::make_pair(name, arg_t(false, necessary, default_value));
    }

    /// @brief Creates a flag argument.
    /// @param is_flag Whether the argument is a flag.
    /// @param necessary Whether the flag is necessary.
    arg_t(bool is_flag, bool necessary)
        : is_flag(is_flag), necessary(necessary), has_default_value(false),
          default_value("") {
    }

    /// @brief Creates a value argument with a default value.
    /// @param is_flag Whether the argument is a flag.
    /// @param necessary Whether the argument is necessary.
    /// @param default_value The default value for the argument.
    /// @throws std::invalid_argument if is_flag is true, as flags cannot have
    /// default values.
    arg_t(bool is_flag, bool necessary, const std::string& default_value)
        : is_flag(is_flag), necessary(necessary), has_default_value(true),
          default_value(default_value) {
        if (is_flag) {
            throw std::invalid_argument("Flag cannot have a default value");
        }
    }

    const bool is_flag;
    const bool necessary;
    const bool has_default_value;
    const std::string default_value;
};

/// @brief Class for parsing command line arguments.
class args_parses_t {
  public:
    args_parses_t(int argc, char* argv[],
                  const std::unordered_map<std::string, arg_t>& allowed_args);

    /// @brief Get the value of a specific argument.
    /// @param arg The name of the argument to get the value for.
    /// @return The value of the argument, or an empty string if the argument is
    /// not found.
    const std::string& get_value(const std::string& arg) const;

    /// @brief Get the value of a specific argument, or throw an exception if it
    /// is not found.
    bool has_arg(const std::string& arg) const;

    /// @brief Get the value of a specific argument, or throw an exception if it
    /// is not found.
    bool has_flag(const std::string& flag) const;

    std::string to_string() const;

  private:
    std::unordered_map<std::string, std::string> value_args;
    std::unordered_set<std::string> flags;
    const std::unordered_map<std::string, arg_t> allowed_args;
};

std::ostream& operator<<(std::ostream& os, const input::args_parses_t& args);

class string_parser {
  public:
    /// @brief Parses a numeric string and checks if it is within the specified
    /// range.
    template<typename T>
    static T parse_numeric(const std::string& s, T min, T max) {
        T value = static_cast<T>(std::stoul(s));
        if (value < min || value > max) {
            throw std::out_of_range("Must be between " + std::to_string(min) +
                                    " and " + std::to_string(max));
        }

        return value;
    }
};

static inline const std::string UNSIGNED_NUMBER_REGEX = R"(^0|[1-9][0-9]*$)";

/// @brief Specialization of parse_numeric for unsigned int.
template<>
inline unsigned int input::string_parser::parse_numeric<unsigned int>(
    const std::string& str, unsigned int min, unsigned int max) {
    if (str.empty() ||
        !std::regex_match(str, std::regex(UNSIGNED_NUMBER_REGEX))) {
        throw std::invalid_argument("Invalid numeric string: " + str);
    }

    unsigned long value = std::stoul(str);
    if (value < min || value > max) {
        throw std::out_of_range("Must be between " + std::to_string(min) +
                                " and " + std::to_string(max));
    }
    return static_cast<unsigned int>(value);
}

/// @brief Specialization of parse_numeric for int.
template<>
inline uint8_t
input::string_parser::parse_numeric<uint8_t>(const std::string& str,
                                             uint8_t min, uint8_t max) {
    return static_cast<uint8_t>(parse_numeric<unsigned int>(str, min, max));
}

/// @brief Specialization of parse_numeric for short.
template<>
inline unsigned long input::string_parser::parse_numeric<unsigned long>(
    const std::string& str, unsigned long min, unsigned long max) {
    if (str.empty() ||
        !std::regex_match(str, std::regex(UNSIGNED_NUMBER_REGEX))) {
        throw std::invalid_argument("Invalid numeric string: " + str);
    }
    unsigned long value = std::stoul(str);
    if (value < min || value > max) {
        throw std::out_of_range("Must be between " + std::to_string(min) +
                                " and " + std::to_string(max));
    }
    return value;
}

/// @brief Specialization of parse_numeric for unsigned short.
template<>
inline unsigned short input::string_parser::parse_numeric<unsigned short>(
    const std::string& str, unsigned short min, unsigned short max) {
    return static_cast<unsigned short>(
        parse_numeric<unsigned int>(str, min, max));
}

/// @brief Parses the input value for a specific argument.
template<typename T>
static T parse_input(const std::string& arg_name, const std::string& val, T min,
                     T max) {
    try {
        return input::string_parser::parse_numeric<T>(val, min, max);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument for " + arg_name + ": " +
                                    e.what());
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range for " + arg_name + ": " +
                                e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing " + arg_name + ": " + e.what());
    }
}

} // namespace input

#endif