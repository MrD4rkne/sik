#ifndef MESSAGES_H
#define MESSAGES_H

#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace messages {
using rational_t = double;
using k_t = uint16_t;
using offset_t = double;

static constexpr size_t PRECISION = 7;

static inline const std::string HELLO_MESSAGE = "HELLO";
typedef struct hello_message {
    std::string player_id;

    std::string to_string() const {
        std::stringstream ss;
        ss << HELLO_MESSAGE << ' ' << player_id;
        return ss.str();
    }

} hello_message_t;

inline std::ostream& operator<<(std::ostream& os, const hello_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string COEFF_MESSAGE = "COEFF";
typedef struct coeff_message {
    std::vector<rational_t> coeffs;

    std::string to_string() const {
        std::stringstream ss;
        ss << "COEFF ";
        for (size_t i = 0; i < coeffs.size(); ++i) {
            ss << std::fixed << std::setprecision(PRECISION) << coeffs[i];
            if (i != coeffs.size() - 1) {
                ss << ' ';
            }
        }
        return ss.str();
    }
} coeff_message_t;

inline std::ostream& operator<<(std::ostream& os, const coeff_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string PUT_MESSAGE = "PUT";
typedef struct put_message {
    k_t point;
    offset_t value;

    std::string to_string() const {
        std::stringstream ss;
        ss << PUT_MESSAGE << ' ' << point << ' ' << std::fixed
           << std::setprecision(PRECISION) << value;
        return ss.str();
    }
} put_message_t;

inline std::ostream& operator<<(std::ostream& os, const put_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string BAD_PUT_MESSAGE = "BAD_PUT";
typedef struct bad_put_message {
    k_t point;
    offset_t value;

    std::string to_string() const {
        std::stringstream ss;
        ss << BAD_PUT_MESSAGE << ' ' << point << ' ' << std::fixed
           << std::setprecision(PRECISION) << value;
        return ss.str();
    }
} bad_put_message_t;

inline std::ostream& operator<<(std::ostream& os,
                                const bad_put_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string STATE_MESSAGE = "STATE";
typedef struct state_message {
    std::vector<rational_t> coeffs;

    std::string to_string() const {
        std::stringstream ss;
        ss << STATE_MESSAGE << ' ';
        for (size_t i = 0; i < coeffs.size(); ++i) {
            ss << std::fixed << std::setprecision(PRECISION) << coeffs[i];
            if (i != coeffs.size() - 1) {
                ss << ' ';
            }
        }
        return ss.str();
    }
} state_message_t;

inline std::ostream& operator<<(std::ostream& os, const state_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string PENALTY_MESSAGE = "PENALTY";
typedef struct penalty_message {
    k_t point;
    offset_t value;

    std::string to_string() const {
        std::stringstream ss;
        ss << PENALTY_MESSAGE << ' ' << point << ' ' << std::fixed
           << std::setprecision(PRECISION) << value;
        return ss.str();
    }
} penalty_message_t;

inline std::ostream& operator<<(std::ostream& os,
                                const penalty_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string SCORING_MESSAGE = "SCORING";
typedef struct scoring_message {
    std::vector<std::pair<std::string, rational_t>> scores;

    std::string to_string() const {
        std::stringstream ss;
        ss << SCORING_MESSAGE << ' ';
        for (size_t i = 0; i < scores.size(); ++i) {
            ss << scores[i].first << ' ' << std::fixed
               << std::setprecision(PRECISION) << scores[i].second;
            if (i != scores.size() - 1) {
                ss << ' ';
            }
        }
        return ss.str();
    }
} scoring_message_t;

inline std::ostream& operator<<(std::ostream& os,
                                const scoring_message_t& msg) {
    os << msg.to_string();
    return os;
}

static inline const std::string TYPES[] = {
    HELLO_MESSAGE, COEFF_MESSAGE,   PUT_MESSAGE,    BAD_PUT_MESSAGE,
    STATE_MESSAGE, PENALTY_MESSAGE, SCORING_MESSAGE};

static const std::string NUMBER_REGEX = "^\\d+$";
static inline k_t deserialize_k(const std::string& str) {
    static const std::regex regex(NUMBER_REGEX);
    if (!std::regex_match(str, regex)) {
        throw std::invalid_argument("Invalid k format");
    }

    try {
        return static_cast<k_t>(std::stoi(str));
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid k value");
    }
}

static const std::string FLOAT_REGEX = "^-?\\d+(\\.\\d{0,7})?$";
static inline offset_t deserialize_offset(const std::string& str) {
    try {
        static const std::regex regex(FLOAT_REGEX);
        if (!std::regex_match(str, regex)) {
            throw std::invalid_argument("Invalid offset format");
        }
        return static_cast<offset_t>(std::stof(str));
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid offset value");
    }
}

template<typename T>
inline std::string serialize_message(const T& message) {
    std::stringstream ss;
    ss << message;
    return ss.str();
}

inline std::string get_type(const std::string& message) {
    std::stringstream ss(message);
    std::string type;
    ss >> type;

    for (const auto& t : TYPES) {
        if (type == t) {
            return type;
        }
    }

    throw std::invalid_argument("Unknown message type: " + type);
}

template<typename T>
inline T deserialize_implementation(const std::vector<std::string>&) {
    static_assert(
        sizeof(T) == 0,
        "deserialize_implementation not implemented for this message type");
}

template<>
inline hello_message_t deserialize_implementation<hello_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() != 2) {
        throw std::invalid_argument("Invalid HELLO message format");
    }

    if (tokens[0] != HELLO_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    hello_message_t msg;
    msg.player_id = tokens[1];
    return msg;
}

template<>
inline coeff_message_t deserialize_implementation<coeff_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        throw std::invalid_argument("Invalid COEFF message format");
    }

    if (tokens[0] != COEFF_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    coeff_message_t msg;
    for (size_t i = 1; i < tokens.size(); ++i) {
        msg.coeffs.push_back(deserialize_offset(tokens[i]));
    }
    return msg;
}

template<>
inline put_message_t deserialize_implementation<put_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() != 3) {
        throw std::invalid_argument("Invalid PUT message format");
    }

    if (tokens[0] != PUT_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    put_message_t msg;
    msg.point = deserialize_k(tokens[1]);
    msg.value = deserialize_offset(tokens[2]);
    return msg;
}

template<>
inline bad_put_message_t deserialize_implementation<bad_put_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() != 4) {
        throw std::invalid_argument("Invalid BAD_PUT message format");
    }

    if (tokens[0] != BAD_PUT_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    bad_put_message_t msg;
    msg.point = deserialize_k(tokens[1]);
    msg.value = deserialize_offset(tokens[2]);
    return msg;
}

template<>
inline state_message_t deserialize_implementation<state_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        throw std::invalid_argument("Invalid STATE message format");
    }

    if (tokens[0] != STATE_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    state_message_t msg;
    for (size_t i = 1; i < tokens.size(); ++i) {
        msg.coeffs.push_back(deserialize_offset(tokens[i]));
    }
    return msg;
}

inline std::pair<k_t, offset_t> deserialize_put(std::string message) {
    std::stringstream ss(message);
    std::vector<std::string> tokens;

    std::string token;
    while (std::getline(ss, token, ' ')) {
        tokens.push_back(token);
    }

    if (tokens.size() != 2) {
        throw std::invalid_argument("Invalid PUT message format");
    }

    k_t point = deserialize_k(tokens[0]);
    offset_t value = deserialize_offset(tokens[1]);
    return {point, value};
}

template<>
inline penalty_message_t deserialize_implementation<penalty_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() != 4) {
        throw std::invalid_argument("Invalid PENALTY message format");
    }

    if (tokens[0] != PENALTY_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    penalty_message_t msg;
    msg.point = deserialize_k(tokens[1]);
    msg.value = deserialize_offset(tokens[2]);
    return msg;
}

template<>
inline scoring_message_t deserialize_implementation<scoring_message_t>(
    const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        throw std::invalid_argument("Invalid SCORING message format");
    }

    if (tokens[0] != SCORING_MESSAGE) {
        throw std::invalid_argument("Invalid message type");
    }

    // SCORING message should have an odd number of tokens,
    // as we have SCORING + n pairs of player_id and score.
    if (tokens.size() % 2 != 1) {
        throw std::invalid_argument("Invalid SCORING message format");
    }

    scoring_message_t msg;
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::string player_id = tokens[i];
        rational_t score = deserialize_offset(tokens[++i]);
        msg.scores.push_back({player_id, score});
    }
    return msg;
}

template<typename T>
inline T deserialize_message(const std::string& message) {
    std::stringstream ss(message);
    std::vector<std::string> tokens;

    std::string token;
    while (std::getline(ss, token, ' ')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 2) {
        throw std::invalid_argument("Too few words in message");
    }

    return deserialize_implementation<T>(tokens);
}

class MessageSender {
  public:
    virtual void
    send_message(const ip::IPAddress ip_address, const std::string& message,
                 const std::function<void(const std::string&)>& callback,
                 uint64_t delay = 0) = 0;

    virtual void send_message(const ip::IPAddress ip_address,
                              const std::string& message, uint64_t delay = 0) {
        static const auto EMPTY = [](const std::string&) {};
        send_message(ip_address, message, EMPTY, delay);
    }

    template<typename T>
    void send_message_serialized(
        const ip::IPAddress ip_address, const T& message,
        const std::function<void(const std::string&)>& callback,
        uint64_t delay = 0) {
        send_message(ip_address, messages::serialize_message(message), callback,
                     delay);
    }

    virtual ~MessageSender() = default;
};

} // namespace messages

#endif