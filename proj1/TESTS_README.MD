# Tests

## How to run

1. Place your code in `script` catalog next to `tests`

```bash
src
tests
prepare.sh
```

2. Navigate to `tests`:

> cd tests

3. Give execute permissions to all scripts

> chmod +x *.sh

4. Run `run.sh`

**Note**: you need to have all ports on range `1440-65565` free. So some tests might fail on students due to bind issues.

Also, for `max_peers` tests you need to have at least two ipv4 adresses
(so probably `127.0.0.1` and from your local network).

## Disclaimer

./tests/test_runner.py was prepared by Jakub Czyszczoń. At no circumstances should you share it without permission from the author.
