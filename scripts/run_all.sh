#!/bin/bash
# Submits every job. Usage: bash scripts/run_all.sh ACCOUNT
if [ -z "$1" ]; then
    echo "Usage: bash scripts/run_all.sh ACCOUNT"
    exit 1
fi
mkdir -p results
sbatch --account=$1 scripts/scaling_1node.sb
for n in 1 2 4 8; do
    sbatch --account=$1 --nodes=$n scripts/scaling_nodes.sb
done
sbatch --account=$1 scripts/census17.sb
squeue -u $USER
