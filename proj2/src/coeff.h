#ifndef COEFF_H
#define COEFF_H

namespace coeff {
class COEFFProvider {
  public:
    virtual size_t get_available_coeffs_count() const = 0;

    virtual bool has_coeffs() const = 0;

    virtual void request_coeffs() = 0;

    virtual const std::string& get_coeffs() = 0;

    virtual void pop_coeffs() = 0;

    virtual ~COEFFProvider() = default;
};
} // namespace coeff

#endif