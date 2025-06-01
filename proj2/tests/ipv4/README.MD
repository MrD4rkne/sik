# IPV4 compability

https://www.cyberciti.biz/faq/how-to-temporarily-disable-ipv6-in-linux/

To disable ipv6:

```bash
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=1
```

To enable it back:

```bash
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=0
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=0
```
