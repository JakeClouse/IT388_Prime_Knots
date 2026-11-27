#!/bin/bash
# Loads the modules and paths that census needs. Use: source scripts/env.sh
PREFIX=$HOME/knots
if command -v module > /dev/null 2>&1; then
    module purge
    module load cpu gcc openmpi cmake
fi
export PATH=$PREFIX/regina/bin:$PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX/regina/lib:$PREFIX/env/lib:$LD_LIBRARY_PATH
export OMP_PLACES=cores
export OMP_PROC_BIND=close
