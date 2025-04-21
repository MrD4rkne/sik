# Cases

## Hello & connect

- sent hello, received connect_ak [_connect_unexpected](./_connect_unexpected.in)
- double connect_ak [_connect_double](./_connect_double.in)
- received connect_ak when not expecting [_hello](./_hello.in)
- received connect from known peer [_hello](./_hello.in)
- wrong hello [bad_msg](./bad_msg.in)
- unexpected hello response (when not send a the start) [hello](./hello.in)
- hello response from unknown peer [_hello](./_hello.in)
- duplicated hello response [_hello](./_hello.in)
- hello_response contains responding peer [_hello_rsp_contains_responded](./_hello_rsp_contains_responded.in)

## Syncing

- unknown peer with sync 255 [sync_start_invalid](./sync_start_invalid)
- unknown peer with sync 254 [sync_start_invalid](./sync_start_invalid)
- unknown peer with sync < 254 [sync_start_invalid](./sync_start_invalid)
- unknown peer with sync = 0 [sync_start_invalid](./sync_start_invalid)
- known peer with sync 255 [sync_start_invalid](./sync_start_invalid)
- known peer with sync 254 [sync_start_invalid](./sync_start_invalid)
- get as a leader [leader_sync](./leader_sync.in)
- sync already in progress with another peer [sync_start_invalid](./sync_start_invalid)
- sync already in progress with the same peer [sync_start_invalid](./sync_start_invalid)
- valid synchronized with unchanged sync [sync_start_invalid](./sync_start_invalid)
- valid synchronized with changed sync [sync_start_invalid](./sync_start_invalid)
- invalid sync with unsynchronized peer [sync_start_invalid](./sync_start_invalid)
- invalid sync with synchronized peer (equal, more than) (./sync_start_invalid)
- timeout delay response [sync_delay_timeout](./sync_delay_timeout.in)
- unexpected delay request/response from known peer [sync_start_invalid](./sync_start_invalid)
- unexpected delay request/response from unknown peer [sync_start_invalid](./sync_start_invalid)
- change synchronize to [sync_start_invalid](./sync_start_invalid)
- desychronize after not receiving sync_start from peer, but from other [desynchronize](./desynchronize.in)
- bad sync from another peer shouldn't desynchronize [desynchronize](./desynchronize.in)
- sending sync_start (but not responding) from peer synced to should lead to not desynchronize [sync_start_from_synchronized](./sync_start_from_synchronized.in)
- send sync to peers known from hello_response
- send sync to peers known from connect
- invalidate sync process when becoming leader (so i do not get stripped of leader when receiving delay_response) [invalidate_sync_when_becoming_leader](./invalidate_sync_when_becoming_leader.in)
- valid sync (time goes up) [sync](./sync.in)
- valid sync (time goes down) [sync](./sync.in)
