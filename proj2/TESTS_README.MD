# Tests

## Correctness

Tester simulates one side of the connection.

### Note

I assume, that:

- `approx_client` does not check if returned `STATE`, `BAD_PUT` and `PENALTY` has values that are consisent with previous client's messages. I only check validating the values (types, out of range).
- message is a valid `PUT k v` \iff `k` is a positive integer `uint16_t` and `v` is a rational number that fits in type `double` and have precision up to 7.
Otherwise I do not treat a message as a `PUT`, but like a unrecognized type like `BAJO JAJO`.
