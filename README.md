# Eraser

Prosty program CLI do nadpisywania (zerowania) urządzeń blokowych lub symulacji procesu kasowania na pliku.

## Uwaga bezpieczeństwa

Kasowanie urządzenia jest **destrukcyjne** i może bezpowrotnie usunąć dane.
Zawsze upewnij się, że wskazujesz poprawne urządzenie (np. `/dev/sdb`, a nie dysk systemowy).

## Funkcje

- Kasowanie blokowe z parametrami `erase_size_MB` i `skip_size_MB`
- Poprawne raportowanie:
  - `Progress` = procent przetworzonego obszaru nośnika
  - `Erased` = procent realnie nadpisanych danych
- Tryb symulacji `--simulate` (bez zapisu)
- Weryfikacja zawartości `--verify-zero` przed kasowaniem
- Tryb raportowy `--verify-only` (bez kasowania)
- Tryb błędów jednolinijkowych `--quiet-errors` / `-q`
- Obsługa przerwania `Ctrl+C`
- Kody wyjścia do automatyzacji skryptowej

## Wymagania

- Linux
- Kompilator C++ z obsługą C++17

## Kompilacja

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic eraser.cpp -o eraser
```

## Użycie

```bash
./eraser <device_or_file> <erase_size_MB> <skip_size_MB> [options]
```

### Argumenty

- `device_or_file` – urządzenie blokowe (np. `/dev/sdb`) albo plik testowy
- `erase_size_MB` – rozmiar fragmentu nadpisywanego zerami w MB (musi być `> 0`)
- `skip_size_MB` – rozmiar pomijanego fragmentu w MB (może być `0`)

### Opcje

- `--simulate` – symulacja (brak zapisu)
- `--verify-zero` – skanuje cel i sprawdza, czy zawiera wyłącznie bajty `00`
  - jeśli znajdzie dane różne od `00`, program pyta: czy rozpocząć kasowanie
- `--verify-only` – wykonuje pełny skan i drukuje raport (bez kasowania)
- `-q`, `--quiet-errors` – jednolinijkowe błędy (bez pełnego helpa)
- `-h`, `--help` – pomoc

> Uwaga: `--verify-only` nie łączy się z `--simulate` ani `--verify-zero`.

## Przykłady

### 1) Pełne kasowanie (bez skip)

```bash
./eraser /dev/sdb 8 0
```

### 2) Kasowanie „co kawałek” (8 MB kasuj, 8 MB pomiń)

```bash
./eraser /dev/sdb 8 8
```

### 3) Symulacja na pliku

```bash
truncate -s 64M test.img
./eraser test.img 4 4 --simulate
```

### 4) Weryfikacja, czy nośnik jest już wyzerowany

```bash
./eraser /dev/sdb 8 0 --verify-zero
```

Jeśli program wykryje bajty różne od `00`, zatrzyma się i zapyta, czy kontynuować kasowanie.

### 5) Tylko raport weryfikacji (bez kasowania)

```bash
./eraser /dev/sdb 1 0 --verify-only
```

Na końcu dostaniesz raport m.in.:
- całkowity rozmiar nośnika
- `Free (00)` – ilość danych równych `00`
- `Used (!00)` – ilość danych różnych od `00`
- czas weryfikacji
- średnią prędkość weryfikacji
- offset pierwszego bajtu różnego od `00` (jeśli istnieje)

### 6) Cichy tryb błędów (do skryptów)

```bash
./eraser test.img abc 4 --simulate --quiet-errors
```

## Interpretacja postępu

- `Progress` pokazuje przebieg po całym nośniku (uwzględnia także pomijane obszary).
- `Erased` pokazuje, jaka część nośnika została realnie nadpisana zerami.

Przy `skip_size_MB > 0` jest normalne, że na końcu:
- `Progress = 100%`
- `Erased < 100%`

## Kody wyjścia

- `0` – sukces
- `2` – niepoprawne argumenty/wartości
- `3` – błąd otwarcia celu
- `4` – błąd pobrania rozmiaru celu
- `5` – błąd pozycjonowania (`lseek`)
- `6` – błąd zapisu
- `7` – operacja anulowana przez użytkownika (odpowiedź „nie” po `--verify-zero`)

## Szybki test

```bash
./eraser --help
./eraser test.img 4 4 --simulate
```
