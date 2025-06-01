# Tester

These tests are coordinated by [`tester.py`](../tester.py) which interprets simple command laungage in file with ext `.in`.

Note that interpreter has been vibe-coded and is pretty chaotic.

See more in [README](./TESTER_README.MD)

## Convention

- testes that starts with `_`, like [`_coeff_max_values.in`](./my/_coeff_max_values.in) are for `approx-client`, without for `approx-server`.
- tests that ends with `_error` like [`_bad_put_error.in`](./my/_bad_put_error.in) are these that the sut should return non-zero exit code.
