# Hybrid CSG: Combining Explicit BREP and Implicit SDF Mesh Generation

## 1. Motivation
Current workflow offers two mutually exclusive options:

* **Implicit CSG (SDF-based)** – enables *smooth* unions/blends via min / smooth_union operations and is converted to geometry with Marching Squares/Cubes.  Resulting meshes have good continuity but poor triangle quality and excessive vertex count.
* **Explicit CSG (BREP-based)** – operates on polygonal primitives and Boolean set operations (union / subtract / intersect).  Topology is exact and triangles are well shaped, but smooth blending is impossible.

The goal is to take *the best of both worlds*: generate a surface that is topologically clean where only hard Boolean primitives are present, yet still allows smooth transitions where they are authored.

## 2. Core Idea
1. **Spatial partition** the modelling volume into *tiles* (aka voxels).  We already have this concept through the `Tile` structure used by `VM::evaluate`.  Each tile comes with:
   * an axis-aligned bounding box (Subgrid → domain in SDF space)
   * an array of SDF sample values at the tile corners
   * **the *minimal* instruction list (`Tile::instructions`) required to evaluate that tile**, produced by `VM::prune_instructions4`.
2. **Classify each tile** by inspecting its pruned instruction list:
   * **Explicit-eligible** – contains only primitives (disk/rect etc.) and *hard* Boolean ops (`Min`, `Max`, possibly `Sub`) but **no** smooth-union expression (`smooth_union`, `Abs`, `Square`, etc.).
   * **Implicit-required** – anything that uses smooth blending or other non-piecewise-linear operators.
3. **Generate geometry** per class:
   * **Explicit tiles** → run polygonal Boolean on the *exact* primitives whose supports intersect the tile.  Output a high-quality BREP mesh chunk.
   * **Implicit tiles** → evaluate SDF on a grid inside the tile and run Marching Squares/Cubes as we already do.
4. **Stitching phase** – weld coincident vertices along tile boundaries so that explicit and implicit chunks form a watertight mesh.

## 3. Conceptual Workflow
- **Author shapes** → **Scalar expression** → **compile()** → **VM::evaluate()**
- **Per-tile (Tile)** → **Classify as Explicit or Implicit**
- **Explicit tiles** → **Gather primitives** → **BREP union**
- **Implicit tiles** → **Sample grid** → **Marching Cubes**
- **Mesh stitching / weld** → **Final Mesh**

## 4. Future Extensions
* **Adaptive Resolution** – implicit tiles that are nearly flat could fall back to explicit representation to further cut polygon count.
* **3-D Marching Cubes** – extend the same classification logic to 3-D voxels easily because instruction pruning works dimension-independently.
* **Parallel Execution** – explicit and implicit tile pipelines are embarrassingly parallel.

---
Once the above steps are implemented we should get a mesh that retains the crisp quality of BREP operations wherever possible, but gracefully degrades to smooth implicit surfaces where the designer intended blends, achieving the *hybrid* CSG objective. 