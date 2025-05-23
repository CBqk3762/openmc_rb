#!/bin/bash

# Output CSV file name
output_csv="benchmarking_results.csv"

# Add headers to the CSV file
echo "Case Name,Tot Init Time (s),Tot Sim Time (s),Transport time (s),Time in Inactive Batches (s),Time in Active Batches (s),Time Accumulating Tallies (s),Tot Time (s),Calc Rate (Inactive) (parts/s),Calc Rate (Active) (parts/s),K-eff (Coll),K-eff (Abs),Combined K-eff,Leakage Frac" > "$output_csv"

# Find all .log files in the current directory and subdirectories
find . -type f -name "*.log" | while read logfile; do
  # Extract file name without path or extension
  filename=$(basename "$logfile" .log)
  
  # Extract the required information from the log file
  initialization_time=$(grep -oP "Total time for initialization\s*=\s*\K[0-9.e+-]+" "$logfile")
  simulation_time=$(grep -oP "Total time in simulation\s*=\s*\K[0-9.e+-]+" "$logfile")
  transport_time=$(grep -oP "Time in transport only\s*=\s*\K[0-9.e+-]+" "$logfile")
  inactive_batches_time=$(grep -oP "Time in inactive batches\s*=\s*\K[0-9.e+-]+" "$logfile")
  active_batches_time=$(grep -oP "Time in active batches\s*=\s*\K[0-9.e+-]+" "$logfile")
  accum_tallies_time=$(grep -oP "Time accumulating tallies\s*=\s*\K[0-9.e+-]+" "$logfile")
  total_elapsed_time=$(grep -oP "Total time elapsed\s*=\s*\K[0-9.e+-]+" "$logfile")
  calc_rate_inactive=$(grep -oP "Calculation Rate \(inactive\)\s*=\s*\K[0-9.e+-]+" "$logfile")
  calc_rate_active=$(grep -oP "Calculation Rate \(active\)\s*=\s*\K[0-9.e+-]+" "$logfile")
  k_eff_collision=$(grep -oP "k-effective \(Collision\)\s*=\s*\K[0-9.e+-]+" "$logfile")
  k_eff_absorption=$(grep -oP "k-effective \(Absorption\)\s*=\s*\K[0-9.e+-]+" "$logfile")
  k_eff_combined=$(grep -oP "Combined k-effective\s*=\s*\K[0-9.e+-]+" "$logfile")
  leakage_fraction=$(grep -oP "Leakage Fraction\s*=\s*\K[0-9.e+-]+" "$logfile")

  # Write the extracted data to the CSV file
  echo "$filename,$initialization_time,$simulation_time,$transport_time,$inactive_batches_time,$active_batches_time,$accum_tallies_time,$total_elapsed_time,$calc_rate_inactive,$calc_rate_active,$k_eff_collision,$k_eff_absorption,$k_eff_combined,$leakage_fraction" >> "$output_csv"
done

echo "CSV file '$output_csv' created successfully."

