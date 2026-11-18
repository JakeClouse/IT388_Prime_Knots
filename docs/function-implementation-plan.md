# Function Implementation Plan

Functions we have to write ourselves to generate and prove all 17-crossing prime knots.
None of these exist in Regina; each one wraps the Regina calls listed in the last column.
Regina's C++ and Python APIs use the same names (checked against the Regina 7.4 engine docs,
https://regina-normal.github.io/engine-docs/).

**Total: 9 functions, about 15 code paths when modes are counted, plus 2 helpers.**

## Functions

| # | Function | Modes / paths | Effort | Why | Regina calls it wraps |
|---|---|---|---|---|---|
| 1 | `gen_alternating` | 1 | Small | Runs plantri, then builds the alternating diagram | plantri (external), `ModelLinkGraph::fromPlantri`, `generateAnyLink`, `Link::makeAlternating` |
| 2 | `flype_canonical` | 1 | **Medium–large** | We write the flype-orbit search; Regina only gives single flypes | `ModelLinkGraph::findFlype`, `flype`, `canonicalPlantri(allowReflection=true)`; `Link::sig` |
| 3 | `switch_crossings` | 1 | Small | Mostly a wrapper; Regina's generator is exhaustive for minimal diagrams | `ModelLinkGraph::generateMinimalLinks`, `Link::change`, `Link::sig` |
| 4 | `reidemeister` | 3 (`extra=0` random, `extra=3` connect, `extra=2` + pass moves) | Medium | Connect mode needs our own signature matching across searches | `Link::r1/r2/r3`, `hasR1/hasR2/hasR3`, `simplifyToLocalMinimum`, `simplifyExhaustive`, `rewrite`, `hasReducingPass`, `size` |
| 5 | `snappy_tri` | 2 (`tries=1`, `tries=300` + verified) | Medium | Retry loop and exception handling; verified mode needs SnapPy, not Regina | `SnapPeaTriangulation(link)`, `solutionType`, `randomise`, `canonise`, `Triangulation<3>::isoSig` |
| 6 | `polynomial` | 2 (`jones`, `homfly`) | Small | Compute, then take the smaller of the knot's and its mirror's polynomial | `Link::jones`, `homflyAZ` / `homflyLM`, `reflect` |
| 7 | `cover_homology` | 2 (laddered k = 5, 6, 7; last-resort k up to 11) | Small–medium | One subgroup per transitive rep into S_k; Magma backend not needed | `Link::group`, `GroupPresentation::simplify`, `enumerateCovers<k>` (k = 2..11), `abelianisation` |
| 8 | `refine` | 1 | Small | Generic group-by-key that finalizes singleton groups; reused after steps 4, 5, 6 and B | none (plain code) |
| 9 | `regina_certify` | 3 (`hyperbolic`, `satellite`, `prime`) | **Large** | Hyperbolic is one call; satellite is a normal-surface torus search; prime has no Regina routine | see below |

### `regina_certify` modes

- **hyperbolic:** `Triangulation<3>::fromIsoSig` or `Link::complement`, then `hasStrictAngleStructure()`
  (one exact LP; `strictAngleStructure()` returns the certificate). Retry on other triangulations if it fails.
- **satellite:** no single Regina call. Build it from:
  - `NormalSurfaces(tri, NormalCoords::Quad, NormalList::Vertex)` (use `Fundamental` for a stronger search)
  - keep surfaces where `isCompact`, `isConnected`, `isOrientable`, `isTwoSided` are true,
    `eulerChar() == 0`, and `isVertexLinking` is false
  - `cutAlong()` → `triangulateComponents()`
  - piece without the cusp: `isSolidTorus()` must be false
  - cusp piece: `isTxI()` must be false; after `idealToFinite()`, `hasCompressingDisc()` must be false
- **prime:** `summands()` only works on closed triangulations, so it cannot be used on a knot complement.
  A composite knot is itself a satellite (swallow-follow torus), so we must show the cusp-side piece
  from the satellite check is not (pair of pants) × S¹, or use a diagram argument.

## Helpers

| Helper | Purpose |
|---|---|
| Census check | Compare final results against the published Regina 17-crossing table |
| Pipeline driver | Batching, checkpointing and parallelism for ~8.5M candidates |

## Notes

- The hard parts are `flype_canonical`, the satellite and prime modes of `regina_certify`,
  and the connect mode of `reidemeister`. Everything else is a thin wrapper.
- Expected total is 8,053,393 = 8,053,363 hyperbolic + 29 satellite + **1 torus knot T(2,17)**.
  T(2,17) is alternating but not hyperbolic, so it needs its own row in the final count.
- Matching isoSigs in step 4 already prove two diagrams are the same knot (up to mirror),
  even if `canonise()` returns a non-canonical answer. Different isoSigs do not prove distinctness.
- Older Regina versions use `intelligentSimplify` instead of `simplify`, and `NS_QUAD` / `NS_VERTEX`
  instead of the `NormalCoords` / `NormalList` enums.
