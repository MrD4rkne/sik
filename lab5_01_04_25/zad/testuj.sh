#!/bin/bash

if [ $# -lt 4 ]; then
  echo "Użycie:"
  echo "  $0 host port files_dir server_dir"
  echo ""
  echo "host       – nazwa lub adres ip serwera"
  echo "port       – port, na którym słucha serwer"
  echo "files_dir  – katalog z plikami do przesłania"
  echo "server_dir – katalog, gdzie serwer umieszcza odebrane pliki"
  echo ""
  echo "Sugeruje się, aby host to ne był localhost."
  echo "Sugeruje się, aby w katalogu files_dir były co najmniej trzy pliki:"
  echo "duży (> 512 MiB), mały (< 256 KiB), pusty."
  exit 1
fi

if [[ $# -eq 5 ]]; then
  sleepTime=$5
else
  sleepTime=1
fi

echo "Sleep time: $sleepTime"s

# Testujemy przesyłanie plików.

for file in $(ls -S "$3"); do
  rm -f "$4/$file"
done

sleep $sleepTime
ls

pids=
for file in $(ls -S "$3"); do
  echo "Wysyłanie pliku $file"
  ./file-client-tcp $1 $2 "$3/$file" &
  pids+="$! "
done

for pid in $pids; do
  wait $pid
done

sleep $sleepTime
ls

for file in $(ls -S "$3"); do
  echo "Porównywanie pliku $file"
  if cmp "$3/$file" "$4/$file" &>/dev/null; then
    echo -e "\e[92mPlik $file przesłany poprawnie\e[0m"
  else
    echo -e "\e[91mPlik $file przesłany niepoprawnie\e[0m"
  fi
done

# Testujemy reakcję serwera, gdy pliki istnieją.

pids=
for file in $(ls -S "$3"); do
  echo "Wysyłanie pliku $file"
  ./file-client-tcp $1 $2 "$3/$file" &
  pids+="$! "
done

for pid in $pids; do
  wait $pid
done

# Testujemy reakcję serwera, gdy klient wyśle mniej, niż oczekuje serwer.

for file in $(ls -S "$3"); do
  rm -f "$4/$file"
done

sleep $sleepTime
ls

pids=
for file in $(ls -S "$3"); do
  if [[ -s "$3/$file" ]]; then
    echo "Wysyłanie pliku $file"
    ./file-client-tcp $1 $2 "$3/$file" -3 &
    pids+="$! "
  fi
done

for pid in $pids; do
  wait $pid
done

sleep $sleepTime
ls

# Testujemy reakcję serwera, gdy klient wyśle więcej, niż oczekuje serwer.

for file in $(ls -S "$3"); do
  rm -f "$4/$file"
done

sleep $sleepTime
ls

pids=
for file in $(ls -S "$3"); do
  echo "Wysyłanie pliku $file"
  ./file-client-tcp $1 $2 "$3/$file" 3 &
  pids+="$! "
done

for pid in $pids; do
  wait $pid
done

sleep $sleepTime
ls

for file in $(ls -S "$3"); do
  echo "Porównywanie pliku $file"
  if cmp "$3/$file" "$4/$file" &>/dev/null; then
    echo -e "\e[92mPlik $file przesłany poprawnie\e[0m"
  else
    echo -e "\e[91mPlik $file przesłany niepoprawnie\e[0m"
  fi
done

# Ponownie testujemy przesyłanie plików.

for file in $(ls -S "$3"); do
  rm -f "$4/$file"
done

sleep $sleepTime
ls

pids=
for file in $(ls -S "$3"); do
  echo "Wysyłanie pliku $file"
  ./file-client-tcp $1 $2 "$3/$file" &
  pids+="$! "
done

for pid in $pids; do
  wait $pid
done

sleep $sleepTime
ls

for file in $(ls -S "$3"); do
  echo "Porównywanie pliku $file"
  if cmp "$3/$file" "$4/$file" &>/dev/null; then
    echo -e "\e[92mPlik $file przesłany poprawnie\e[0m"
  else
    echo -e "\e[91mPlik $file przesłany niepoprawnie\e[0m"
  fi
done
