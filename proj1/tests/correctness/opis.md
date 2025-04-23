## Add new socket
socket `sockname` `port`

## Add new host
host `hostname` `ip` `port`

## Send
send `sockname` `hostname` `format` `data` `(!)`

- `format` as described [here](https://docs.python.org/3/library/struct.html#format-characters).
- `data` as comma serparated integers
- if `!` is present, log packet as ERROR MSG `packet`

## Receive
receive `sockname` `hostname` `timeout` `fomat` `data comparison keys` `(!)`

- if `hostname` is equal to `None`, receive from any host
- if `timeout` is `None`, don't set a timeout, else set to `float(timeout)` seconds
- `format` as described [here](https://docs.python.org/3/library/struct.html#format-characters).
- `data comparison keys` are coma separated keys as below:
  - `[n_1:int;n_2:int;...]` match to any int in array
  - `(lower:int;upper:int)` match `lower <= value <= upper`
  - `*` match any
  - `n:int` match `value == int(n)`
- if `!` is present, log packet as ERROR MSG `packet`
  
# Sleep
sleep `seconds`

- sleep for `float(seconds)`

## Comments
empty lines and lines starting with `#` are ignored.