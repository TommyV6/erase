### README Content (in English and Polish)

```markdown
# Device Block Eraser

## Description (English)
This C++ program is designed to securely erase data on storage devices under Linux by overwriting specific blocks in a defined pattern. Instead of overwriting the entire device, it alternates between writing and skipping blocks, making it efficient for selective data removal. 

Key Features:
- Overwrites data in chunks with configurable sizes.
- Skips blocks to save time and resources.
- Displays real-time progress and speed.
- Allows interruption with `Ctrl+C` while cleaning up resources.

### Requirements
- Linux system with GCC (or equivalent compiler).
- Administrative privileges (`sudo`) to access block devices.

### Usage
```bash
sudo ./erase <device> <erase_size_MB> <skip_size_MB>
```

Example:
```bash
sudo ./erase /dev/sdd 1 10
```
This command overwrites the first 1 MB of every 10 MB on the device `/dev/sdd`.

### Compilation
Use the following command to compile:
```bash
g++ -o erase erase.cpp
```

---

## Opis (Polski)
Ten program w języku C++ służy do bezpiecznego kasowania danych na urządzeniach pamięci masowej w systemie Linux, nadpisując wybrane bloki według określonego wzorca. Zamiast nadpisywać cały dysk, program naprzemiennie zapisuje i omija bloki, co pozwala na efektywne usuwanie danych.

Kluczowe funkcje:
- Nadpisuje dane w blokach o konfigurowalnych rozmiarach.
- Omija bloki, oszczędzając czas i zasoby.
- Wyświetla postęp w czasie rzeczywistym oraz prędkość działania.
- Obsługuje przerwanie działania za pomocą `Ctrl+C`, sprzątając zasoby.

### Wymagania
- System Linux z GCC (lub równoważnym kompilatorem).
- Uprawnienia administracyjne (`sudo`) do dostępu do urządzeń blokowych.

### Użycie
```bash
sudo ./erase <device> <erase_size_MB> <skip_size_MB>
```

Przykład:
```bash
sudo ./erase /dev/sdd 1 10
```
Polecenie to nadpisze pierwsze 1 MB co każde 10 MB na urządzeniu `/dev/sdd`.

### Kompilacja
Aby skompilować program, użyj polecenia:
```bash
g++ -o erase erase.cpp
```
``` 
