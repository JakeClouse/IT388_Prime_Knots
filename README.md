# IT 388 Prime Knots

Generates and proves every prime knot with up to 17 crossings using Regina.
MPI splits the work across nodes and OpenMP splits it across the cores of each node.

## How it works

`census N` does five steps for every crossing number from 3 to N:

1. **Generate**: plantri lists the graphs, `flype_canonical` keeps one per flype class,
   and `switch_crossings` makes the non-alternating diagrams. Each rank takes an equal
   share of the graphs.
2. **Remove duplicates**: each knot is sent to the rank that owns its complement
   signature, and diagrams with the same signature are merged.
3. **Split by invariants**: knots are grouped by Jones polynomial, then by cover
   homology. Knots left alone in a group are proven different from all others.
4. **Prove hyperbolic**: a strict angle structure proves a knot is hyperbolic, so prime.
5. **Non-hyperbolic**: torus knots and satellites are recognised; unknots, composites
   and smaller knots are dropped.

Rank 0 prints the counts next to the known table and adds a line to the timing CSV.

## On Expanse

Setup, once:

```bash
git clone https://github.com/JakeClouse/IT388_Prime_Knots.git
cd IT388_Prime_Knots
bash scripts/setup_expanse.sh
```

Run everything:

```bash
bash scripts/run_all.sh YOUR_ACCOUNT
```

This submits the scaling jobs and the 17 crossing census. Results go in `results/`.

Make the graphs when the jobs finish:

```bash
python3 scripts/plot.py results/scaling_1node.csv results/scaling_nodes.csv
```

## Quick test

```bash
source scripts/env.sh
cd build && ctest --output-on-failure
```
