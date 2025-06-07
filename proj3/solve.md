# Zadanie 3

## Autor

Marcin Szopa 459531

## a)

![Diagram sieci](./network.svg)

## b)

traceroute c96.mimuw.edu.pl
traceroute to c96.mimuw.edu.pl (193.0.103.36), 30 hops max, 60 byte packets
 1  palo.mimuw.edu.pl (193.0.94.114)
 2  c96.mimuw.edu.pl (193.0.103.36)

## c)

### Tabela trasowania (routing table)

| Sieć docelowa    | Maska podsieci     | Brama (Gateway)      | Interfejs      |
|------------------|--------------------|----------------------|----------------|
| 192.0.103.0      | /27                | 0.0.0.0              | eth0           |
| 192.0.103.32     | /27                | 0.0.0.0              | eth1           |
| 192.0.94.0       | /24                | 0.0.0.0              | eth2           |
| 193.0.115.0      | /24                | 0.0.0.0              | eth3           |
| 193.0.96.0       | /24                | 192.0.103.36         | eth1           |

## d)

```dns
$TTL 86400 ; 1d

mimuw.edu.pl. IN SOA ns3.mimuw.edu.pl. uw.edu.pl. (
2025060712 ; numer seryjny
2600 ; odświeżanie 6h
1200 ; ponawianie 20min
604800 ; wygasanie 1w
7200 ; negatywne buforowanie 2h )

IN NS ns3.mimuw.edu.pl.

students IN A 192.0.96.30

c96 IN A 192.0.96.31
c96 IN A 192.0.103.36

palo IN A 193.0.103.37
palo IN A 193.0.94.114
palo IN A 193.0.115.237
palo IN A 193.0.103.16

kampus IN A 193.0.115.236

wa1 IN A 192.0.103.15
```
