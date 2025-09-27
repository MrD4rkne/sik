# Zadania z sieci

Na pewnym komputerze wykonano polecenie traceroute kampus.uw.edu.pl i otrzymano następujący wynik (czasy odpowiedzi zostały pominięte):

traceroute to kampus.uw.edu.pl (193.0.115.236), 30 hops max, 60 byte packets
 1  _gateway (192.168.2.1)
 2  * * *
 3  * * *
 4  * * *
 5  wa1.equinix.com (195.182.218.53)
 6  palo.uw.edu.pl (193.0.103.16)
 7  kampus.uw.edu.pl (193.0.115.236)

Polecenie traceroute kampus.uw.edu.pl wykonano również na komputerze students.mimuw.edu.pl z wynikiem:

traceroute kampus.uw.edu.pl
traceroute to kampus.uw.edu.pl (193.0.115.236), 30 hops max, 60 byte packets
 1  c96.mimuw.edu.pl (193.0.96.31)
 2  palo.uw.edu.pl (193.0.103.37)
 3  kampus.uw.edu.pl (193.0.115.236)

Na students.mimuw.edu.pl wykonano także polecenie traceroute ns3.uw.edu.pl i otrzymano:

traceroute to ns3.uw.edu.pl (193.0.94.133), 30 hops max, 60 byte packets
 1  c96.mimuw.edu.pl (193.0.96.31)
 2  palo.uw.edu.pl (193.0.103.37)
 3  ns3.uw.edu.pl (193.0.94.133)

Przyjmijmy następujące oznaczenia:

    wa1 – wa1-ix.equinix.com
    palo – palo.uw.edu.pl
    kampus – kampus.uw.edu.pl
    ns3 – ns3.uw.edu.pl
    students – students.mimuw.edu.pl
    c96 – c96.mimuw.edu.pl

a) Narysuj logiczny schemat połączeń serwerów kampus, ns3, students i ruterów wa1, palo i c96. Przydziel nazwy interfejsów i ich adresy IP wraz z maskami sieciowymi. Tam gdzie to możliwe, przypisz maskę /24. Uzupełnij brakujące informacje w taki sposób, aby były spójne z wyżej przedstawionymi wynikami polecenia traceroute i przedstawionym schematem połączeń.

b) Napisz wynik polecenia traceroute przy śledzeniu trasy z komputera ns3.uw.edu.pl do rutera c96.mimuw.edu.pl przy założeniu, że pakiety przechodzą przez wymienione w treści rutery.

c) Napisz fragment tablicy tras rutera palo umożliwiający wyżej przedstawioną komunikację. Wszystkie wpisy w tablicy tras mają być jawne i wyrażone numerycznie. Przyjmij, że trasa domyślna prowadzi do rutera 193.0.103.1.

d) Podaj fragment zawartości pliku strefy uw.edu.pl zawierającego wszystkie odwzorowania nazw na adresy pojawiające się w treści zadania i w rozwiązaniu. Przyjmij, że dla tej strefy serwerem DNS jest ns3. Uwzględnij rekord SOA.

Jako rozwiązanie umieść w Moodle plik w formacie PDF. Rozwiązanie powinno zawierać imię, nazwisko i numer indeksu autora lub autorki.
