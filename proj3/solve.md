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
