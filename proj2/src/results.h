#ifndef RESULTS_H
#define RESULTS_H

#include <memory>
#include <stdexcept>
#include <string>

namespace results {

/// @brief A class representing the result of an operation.
/// It can be either a success or a failure.
/// If it is a failure, it contains an error message.
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

/// @brief A class representing the result of an operation with a value.
/// It can be either a success with a value or a failure with an error message.
/// If it is a failure, it contains an error message.
/// If it is a success, it contains a value of type T.
/// @tparam T The type of the value.
template<typename T>
class TypedResult {

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

        return *value;
    }

    const std::string& get_error_message() const {
        return error_message;
    }

  private:
    explicit TypedResult(const std::string& error_message = "")
        : error_message(error_message) {
    }

    explicit TypedResult(T value) : value(std::make_unique<T>(value)) {
    }

    std::string error_message;
    std::unique_ptr<T> value = nullptr;
};

} // namespace results

#endif