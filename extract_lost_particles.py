import re
import csv
from collections import defaultdict

# File path to your OpenMC log file
log_path = "/home/cbyers/Projects/openmc/delta-tracking-tests/assembly/assembly_reflective_Del_leaks.log"
csv_path = "/home/cbyers/Projects/openmc/delta-tracking-tests/assembly/lost_assembly_particles.csv"

loss_reasons = [
    "Could not find the cell containing particle",
    "Particle lost after surface crossing.",
    "Could not find cell after boundary advancement.",
    "After particle"
]

lost_particles = []
current_particle = {}

with open(log_path, "r") as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    line = line.strip()

    # Detect loss reason
    if any(reason in line for reason in loss_reasons) or line.startswith("[LOSS]"):
        # Extract loss reason from line if it starts with [LOSS]
        if "[LOSS]" in line:
            loss_match = re.match(r"\[LOSS\]\s*(.*)", line)
            if loss_match:
                current_particle = {"cause": loss_match.group(1).strip()}
        else:
            current_particle = {"cause": line.strip()}

        continue  # move to next line to gather more details

    # Extract Particle ID
    if "Particle ID:" in line:
        try:
            current_particle["particle_id"] = int(line.split(":")[1].strip())
        except ValueError:
            continue

    # Extract level and geometry info
    if line.startswith("Level"):
        level_match = re.match(
            r"Level (\d+) \| r = \(([^)]+)\) \| cell = (-?\d+) \| universe = (-?\d+) \| lattice = (-?\d+) \| lattice_index = \(([^)]+)\)",
            line
        )
        if level_match:
            try:
                current_particle["level"] = int(level_match.group(1))
                r = tuple(map(float, level_match.group(2).split(", ")))
                current_particle["r_x"] = r[0]
                current_particle["r_y"] = r[1]
                current_particle["r_z"] = r[2]
                current_particle["cell"] = int(level_match.group(3))
                current_particle["universe"] = int(level_match.group(4))
                current_particle["lattice"] = int(level_match.group(5))
                current_particle["lattice_index"] = level_match.group(6)
                lost_particles.append(current_particle)
                current_particle = {}  # Reset for next particle
            except Exception:
                continue

# Save to CSV
fieldnames = ["cause", "particle_id", "level", "r_x", "r_y", "r_z", "cell", "universe", "lattice", "lattice_index"]
with open(csv_path, "w", newline="") as csvfile:
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for particle in lost_particles:
        writer.writerow(particle)

print(f"Saved {len(lost_particles)} lost particle entries to {csv_path}")
