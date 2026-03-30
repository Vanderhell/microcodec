# microcodec — Finalizačný agent prompt (po v0.5.0)

> Použiť po dokončení v0.5.0 (všetky algoritmy + testy).
> Implementuje: dispatch API, integration testy, README, v1.0.0.

---

## Stav pred spustením

```
v0.1.0  RLE       20/20 tests  ✓
v0.2.0  Varint    25/25 tests  ✓
v0.3.0  Delta     25/25 tests  ✓
v0.4.0  LZSS      25/25 tests  ✓
v0.5.0  Huffman   25/25 tests  ✓
```

---

## KROK A — src/microcodec.c + tests/test_integration.c

```
Implement dispatch API exactly as in microcodec_spec.md §11:
  mc_encode, mc_decode, mc_max_encoded_size, mc_alg_name

Then implement tests/test_integration.c with all 20 tests
from microcodec_spec.md §13.6.

Key tests to verify:
  - All algorithms via dispatch API work correctly
  - MC_ERR_DISABLED returned when algorithm compiled out
  - mc_max_encoded_size(alg, n) >= actual encoded size for all alg
  - Delta+LZSS pipeline round-trip on 1000 float readings
  - Delta+Huffman pipeline round-trip
  - Compression ratio test: delta+lzss on temperature > 3:1
  - All algorithms handle 0-length and 1-byte input

After all 20 integration tests pass:
  cmake --build build --clean-first  → zero warnings
  ctest --test-dir build -C Debug --output-on-failure  → ALL pass

Commit: test(integration): complete integration suite (20/20 passing)
Tag: v1.0.0
```

---

## KROK B — README.md

README musí obsahovať:

```markdown
# microcodec

> Five compression algorithms for embedded sensor data.
> Zero allocations. Zero dependencies. #include and go.

[![Language: C99](badge)]
[![License: MIT](badge)]
[![Tests: 120+](badge)]

## Why microcodec?
## Quick start
## Algorithms

| Algorithm | Best for | Typical ratio | Notes |
|-----------|----------|---------------|-------|
| RLE | Repeated bytes, padding, zeros | 10–50× for uniform data | Simplest, fastest |
| Varint | Small integers, IDs, counters | 2–4× | Exact for small values |
| Delta | Slowly-changing sensor values | 3–8× | Float quantization |
| LZSS | Binary blobs, config data | 2–4× | General purpose |
| Static Huffman | Structured binary data | 1.5–3× | Pre-built table |

## Configuration
## API reference
## Integration with microdb
## Design decisions
## Part of micro-toolkit
## License
```

---

## KROK C — Finálny checklist

```bash
cmake --build build --config Debug --clean-first
ctest --test-dir build -C Debug --output-on-failure

# Skontroluj:
grep -r "printf" src/ include/ && echo "FAIL" || echo "OK"
grep -r "malloc\|free" src/ && echo "FAIL" || echo "OK"

# Očakávané test suites:
# test_rle           20/20
# test_varint        25/25
# test_delta         25/25
# test_lzss          25/25
# test_huff          25/25
# test_integration   20/20
# test_lzss_win64    (variant)
# test_rle_disabled  (variant)
# Celkom: 120+ testov
```

```bash
git add -A
git commit -m "docs: add README and finalize v1.0.0"
git tag v1.0.0
git push origin main --tags
```

---

## Čo agent NESMIE robiť

```
✗ Neskáč na KROK A kým v0.5.0 nie je otagovaný
✗ Žiadny malloc/free kdekoľvek v src/
✗ Žiadny printf v src/ ani include/
✗ Netaguj v1.0.0 kým všetky testy neprechádzajú
```

---

*microcodec finalization guide*
*github.com/Vanderhell/microcodec*
