# Tests

## Correctness

Tester simulates one side of the connection.

### Note

I assume that `approx_client` does not check if returned `STATE`, `BAD_PUT` and `PENALTY` has values that are consisent with previous client's messages. I only check validating the values (types, out of range).
