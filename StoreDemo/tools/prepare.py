"""Generate reproducible external distribution resources (not part of AST)."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / "dist/resources"
root.mkdir(parents=True, exist_ok=True)
(root / "message.txt").write_bytes(b"ASTER_STORE_RESOURCE_V1\n")
with (root / "pattern.bin").open("wb") as stream:
    for start in range(0, 1048576, 4096):
        stream.write(bytes((start+i) % 251 for i in range(4096)))
print(root)
