import re
import os
from pathlib import Path
import pandas as pd
import sys

# Load the log file
log_file = Path(sys.argv[1])
out_dir = log_file.with_name(log_file.stem + "_particles")
out_dir.mkdir(exist_ok=True)

log_text = log_file.read_text()

# Pattern to extract coord blocks
pattern = re.compile(
    r"\[Coord Update\] (?P<context>[^\n]+)\n\s+Particle ID: (?P<id>\d+)\n(?P<coords>(?:\s+Level.*\n)+)",
    re.MULTILINE
)

# Organise particle data
particle_data = {}

for match in pattern.finditer(log_text):
    particle_id = match.group("id")
    context = match.group("context")
    coords = match.group("coords").strip().splitlines()

    for line in coords:
        level_match = re.match(
            r"\s+Level (?P<level>\d+) \| r = \((?P<x>[^,]+), (?P<y>[^,]+), (?P<z>[^\)]+)\) \| cell = (?P<cell>\-?\d+)"
            r" \| universe = (?P<univ>\-?\d+) \| lattice = (?P<lattice>\-?\d+) \| lattice_index = \((?P<lx>\-?\d+), (?P<ly>\-?\d+), (?P<lz>\-?\d+)\)",
            line
        )
        if level_match:
            row = {
                "context": context,
                "level": int(level_match.group("level")),
                "x": float(level_match.group("x")),
                "y": float(level_match.group("y")),
                "z": float(level_match.group("z")),
                "cell": int(level_match.group("cell")),
                "universe": int(level_match.group("univ")),
                "lattice": int(level_match.group("lattice")),
                "lattice_i": f"({level_match.group('lx')},{level_match.group('ly')},{level_match.group('lz')})"
            }
            particle_data.setdefault(particle_id, []).append(row)

# Save one CSV per particle
for pid, rows in particle_data.items():
    df = pd.DataFrame(rows)
    df.to_csv(out_dir / f"particle_{pid}.csv", index=False)

print(f"Done: extracted {len(particle_data)} particles to '{out_dir}'")
