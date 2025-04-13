#ifndef RESULTS_H
#define RESULTS_H

#include <stdexcept>
#include <string>

namespace results {

class Result {
  public:
    static Result Success() {
        return Result();
    }

    static Result Failure(const std::string& error_message) {
        return Result(error_message);
    }

    bool is_success() const noexcept {
        return error_message.empty();
    }

    const std::string& get_error_message() const {
        return error_message;
    }

  private:
    explicit Result(const std::string& error_message = "")
        : error_message(error_message) {
    }

    std::string error_message;
};

template<typename T>
class TypedResult {
    static_assert(std::is_default_constructible<T>::value,
                  "T must be default constructible");

  public:
    static TypedResult<T> Success(T value) {
        return TypedResult<T>(value);
    }

    static TypedResult<T> Failure(const std::string& error_message) {
        return TypedResult<T>(error_message);
    }

    bool is_success() const noexcept {
        return error_message.empty();
    }

    T get_value() const {
        if (!is_success()) {
            throw std::runtime_error("Cannot get value from a failed result");
        }
        return value;
    }

    const std::string& get_error_message() const {
        return error_message;
    }

  private:
    explicit TypedResult(const std::string& error_message = "")
        : error_message(error_message) {
    }

    explicit TypedResult(T value) : value(value) {
    }

    std::string error_message;
    T value{};
};

} // namespace results

#endif