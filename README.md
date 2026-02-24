# Eraser

Program CLI do nadpisywania (zerowania) urządzeń blokowych lub symulacji procesu kasowania na pliku.

Program działa w cyklu:
- zapisuje `erase_size_MB` bajtów `00`,
- pomija `skip_size_MB` bez zapisu,
- powtarza ten wzór do końca nośnika.

To podejście przyspiesza operację względem pełnego nadpisania całego dysku, ale przy `skip_size_MB > 0` nie kasuje wszystkich danych. W praktyce:
- utrudnia odzysk danych z pominiętych obszarów,
- może być wystarczające dla szybkiego „osłabienia” danych,
- nie jest równoważne pełnemu, kryptograficznie bezpiecznemu wymazaniu całego nośnika.

Program jest najbardziej sensowny dla dysków HDD i części starszych SSD SATA. Dla NVMe zwykle lepiej użyć funkcji kontrolera (`Format` / `Sanitize`) przez `nvme-cli`.

## Uwaga bezpieczeństwa

Kasowanie urządzenia jest **destrukcyjne** i może bezpowrotnie usunąć dane.
Zawsze upewnij się, że wskazujesz poprawne urządzenie (np. `/dev/sdb`, a nie dysk systemowy).

## Jak dokładnie kasuje `eraser`

1. Odczytuje rozmiar urządzenia.
2. W pętli ustawia offset i zapisuje zera tylko na wybranych fragmentach.
3. Dla ostatniego fragmentu zapisuje tylko tyle bajtów, ile zostało do końca nośnika.
4. Liczy i pokazuje dwa wskaźniki:
  - `Progress` – ile nośnika zostało już „przejrzane” (z uwzględnieniem skipów),
  - `Erased` – ile danych realnie nadpisano zerami.

### Co to daje w praktyce

- `skip_size_MB = 0` → pełne nadpisanie całego nośnika zerami.
- `skip_size_MB > 0` → szybciej, ale częściowe nadpisanie (kompromis czas vs kompletność).

### Ograniczenia

- Na SSD wewnętrzne mechanizmy FTL/wear-leveling mogą powodować, że logiczne nadpisanie nie zawsze mapuje 1:1 na fizyczne komórki.
- Dla SSD/NVMe do finalnego wymazywania nośnika preferowane są komendy kontrolera (`Format/Sanitize`).

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

## Kasowanie dysków NVMe (`nvme-cli`)

Jeśli masz dysk NVMe, zwykle lepsze jest wymazywanie sprzętowe kontrolera.

### 1) Instalacja i identyfikacja

```bash
sudo apt install nvme-cli
sudo nvme list
```

Przykładowe urządzenie: `/dev/nvme0n1`

### 2) Sprawdzenie możliwości kontrolera

```bash
sudo nvme id-ctrl /dev/nvme0 | less
```

Szukaj pól związanych z `Format NVM` i `Sanitize`.

### 3) Szybkie wymazanie logiczne (często wystarczające operacyjnie)

```bash
sudo nvme format /dev/nvme0n1 --ses=0
```

### 4) Secure Erase przez `format` (jeśli wspierane)

```bash
sudo nvme format /dev/nvme0n1 --ses=1
```

### 5) Cryptographic Erase (najszybsze przy self-encrypting drive)

```bash
sudo nvme format /dev/nvme0n1 --ses=2
```

### 6) Sanitize (najbardziej „sprzętowe” czyszczenie, jeśli dostępne)

```bash
sudo nvme sanitize /dev/nvme0n1 --sanact=1
sudo nvme sanitize-log /dev/nvme0n1
```

> Bardzo ważne:
> - Używaj poprawnego urządzenia (`/dev/nvmeXnY`), bo operacja jest nieodwracalna.
> - Niektóre dyski wymagają odblokowania/wyłączenia „frozen state” (czasem pomaga pełny restart).
> - Dobór `--ses` i `--sanact` zależy od wsparcia konkretnego modelu NVMe.

## Szybki test

```bash
./eraser --help
./eraser test.img 4 4 --simulate
```
