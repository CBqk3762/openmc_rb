#!/bin/bash

# Usage: ./extract_coords.sh path/to/logfile.log

LOGFILE="$1"
if [[ ! -f "$LOGFILE" ]]; then
  echo "Error: file not found: $LOGFILE"
  exit 1
fi

OUTDIR="${LOGFILE%.log}_particles"
mkdir -p "$OUTDIR"

# Awk script to extract blocks per particle and write CSVs
awk -v outdir="$OUTDIR" '
BEGIN {
  FS = "[()|,]+"; OFS = ",";
}
$0 ~ /^\[Coord Update\]/ {
  context = substr($0, index($0,$3));
  getline;
  split($0, idparts, /: /);
  particle_id = idparts[2];
  next;
}
$0 ~ /^\s+Level/ {
  level = $2;
  x = $5; y = $6; z = $7;
  cell = $10;
  universe = $13;
  lattice = $16;
  lx = $18; ly = $19; lz = $20;

  filename = outdir "/particle_" particle_id ".csv";
  if (!(filename in seen)) {
    print "context", "level", "x", "y", "z", "cell", "universe", "lattice", "lattice_i" > filename;
    seen[filename] = 1;
  }
  printf "\"%s\",%s,%s,%s,%s,%s,%s,%s,\"(%s,%s,%s)\"\n", context, level, x, y, z, cell, universe, lattice, lx, ly, lz >> filename;
}
' "$LOGFILE"

echo "Done. CSVs written to: $OUTDIR"
