# IT 388 Prime Knots

Generates and proves every prime knot with up to 17 crossings using Regina,
with MPI across nodes and OpenMP threads inside each MPI rank.

## Pipeline

`census N` runs these stages for all crossing numbers 3 to N:

1. **Generate**: plantri lists the 4-valent planar graphs (split across ranks),
   `flype_canonical` keeps one graph per flype class, and `switch_crossings`
   makes the non-alternating diagrams. Diagrams that simplify are dropped.
   Each diagram gets the isomorphism signature of its knot complement from SnapPea.
2. **Dedup**: knots are sent to the rank that owns their signature (MPI_Alltoallv)
   and diagrams with the same complement are merged.
3. **Refine**: knots are sent to the rank that owns their Jones polynomial, then
   split by cover homology (index 3 to 6). Leftover groups are merged by retrying SnapPea.
4. **Certify**: each distinct knot gets a strict angle structure (hyperbolic, so prime).
   Alternating knots are proven by the Tait flyping theorem.
5. **Non-hyperbolic**: torus knots and satellites are recognised and proven prime.

At the end rank 0 compares the counts with the known table and appends a
timing line to the CSV file.

## Build and test on Expanse

```bash
git clone https://github.com/JakeClouse/IT388_Prime_Knots.git
cd IT388_Prime_Knots
bash scripts/setup_expanse.sh
```

This installs plantri and Regina 7.4.1 under `$HOME/knots` and builds the
project in `build/`. It only needs to be run once.

Quick check on an interactive node:

```bash
srun --partition=debug --account=YOUR_ACCOUNT --nodes=1 --ntasks=4 --cpus-per-task=4 --time=00:20:00 --pty bash
source scripts/env.sh
cd build && ctest --output-on-failure && cd ..
export OMP_NUM_THREADS=4
srun --mpi=pmi2 build/census 12
```

## Running

Set `--account` in the `.sb` files first.

| Job | Command |
|---|---|
| Strong scaling on 1 node and problem sizes 11 to 14 | `sbatch scripts/scaling_1node.sb` |
| Scaling across nodes | `for n in 1 2 4 8; do sbatch --nodes=$n scripts/scaling_nodes.sb; done` |
| Full census up to 17 crossings | `sbatch scripts/census17.sb` |

Usage: `census MAX_CROSSINGS [--plantri PATH] [--csv FILE] [--tries N]`

Output is a table of proven and unresolved knots per crossing number, checked
against the known counts. Timings go to the CSV file.

## Plots

```bash
python3 scripts/plot.py results/scaling_1node.csv results/scaling_nodes.csv
```

This prints speedup and efficiency and writes time, speedup, efficiency and
per-stage plots next to each CSV. Speedup is T(1)/T(p); for the node runs the
1-node time is the baseline.
