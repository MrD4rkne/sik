#ifndef INPUT_H
#define INPUT_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdexcept>

namespace input{

struct arg_t{
    static auto get_flag(const std::string& name, bool necessary = false){
        return std::make_pair(name, arg_t(true, necessary));
    }

    static auto get_arg(const std::string& name, bool necessary = true, const std::string& default_value = ""){
        return std::make_pair(name, arg_t(false, necessary, default_value));
    }

    arg_t(bool is_flag, bool necessary) :
        is_flag(is_flag), necessary(necessary), has_default_value(false), default_value("") {}

    arg_t(bool is_flag, bool necessary, const std::string& default_value) :
        is_flag(is_flag), necessary(necessary), has_default_value(true), default_value(default_value) {
        if (is_flag) {
                throw std::invalid_argument("Flag cannot have a default value");
        }
    }

    const bool is_flag;
    const bool necessary;
    const bool has_default_value;
    const std::string default_value;
};

class args_parses_t{
    public:
        args_parses_t(int argc, char* argv[], const std::unordered_map<std::string, arg_t>& allowed_args);

        const std::string& get_value(const std::string& arg) const;

        bool has_arg(const std::string& arg) const;

        bool has_flag(const std::string& flag) const;
        
        std::string to_string() const;

    private:
        std::unordered_map<std::string, std::string> value_args;
        std::unordered_set<std::string> flags;
        const std::unordered_map<std::string, arg_t> allowed_args;
};

std::ostream& operator<<(std::ostream& os, const input::args_parses_t& args);

} // namespace input

#endif