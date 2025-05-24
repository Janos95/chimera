#include "marching_squares.h"
#include "vm.h" 
#include <cmath>
#include <cstdio>
#include <vector> 
#include <unordered_map>

// Interpolate the zero crossing between two values
static float interpolate(float v1, float v2) {
    if (std::abs(v1 - v2) < 1e-6f) printf("interpolate: %f %f\n", v1, v2);
    return -v1 / (v2 - v1);
}

// Hasher for edge key pairs
struct Hasher {
    size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
        return std::hash<uint64_t>{}(uint64_t(p.first) | (uint64_t(p.second) << 32));
    }
};

// Edge indices for marching squares lookup
struct EdgeIndices {
    int i1, j1, i2, j2;
};

// Lookup table for marching squares configurations
static const std::array<std::array<EdgeIndices, 2>, 16> marching_squares_table = {{
    // Case 0 (0000): All positive
    {{{-1, -1, -1, -1}, {-1, -1, -1, -1}}},
    // Case 1 (0001): TL negative. Connect Left, Top.
    {{{0, 2, 0, 1}, {-1, -1, -1, -1}}},
    // Case 2 (0010): TR negative. Connect Top, Right.
    {{{0, 1, 1, 3}, {-1, -1, -1, -1}}},
    // Case 3 (0011): TL, TR negative. Connect Left, Right.
    {{{0, 2, 1, 3}, {-1, -1, -1, -1}}},
    // Case 4 (0100): BL negative. Connect Left, Bottom.
    {{{0, 2, 2, 3}, {-1, -1, -1, -1}}},
    // Case 5 (0101): TL, BL negative. Connect Top, Bottom.
    {{{0, 1, 2, 3}, {-1, -1, -1, -1}}},
    // Case 6 (0110): TR, BL negative. Ambiguous. Connect (Top, Left) and (Bottom, Right).
    {{{0, 1, 0, 2}, {2, 3, 1, 3}}},
    // Case 7 (0111): TL, TR, BL negative. Connect Bottom, Right.
    {{{2, 3, 1, 3}, {-1, -1, -1, -1}}},
    // Case 8 (1000): BR negative. Connect Bottom, Right.
    {{{2, 3, 1, 3}, {-1, -1, -1, -1}}},
    // Case 9 (1001): TL, BR negative. Ambiguous. Connect (Top, Right) and (Bottom, Left).
    {{{0, 1, 1, 3}, {2, 3, 0, 2}}},
    // Case 10 (1010): TR, BR negative. Connect Top, Bottom.
    {{{0, 1, 2, 3}, {-1, -1, -1, -1}}},
    // Case 11 (1011): TL, TR, BR negative. Connect Left, Bottom.
    {{{0, 2, 2, 3}, {-1, -1, -1, -1}}},
    // Case 12 (1100): BL, BR negative. Connect Left, Right.
    {{{0, 2, 1, 3}, {-1, -1, -1, -1}}},
    // Case 13 (1101): TL, BL, BR negative. Connect Top, Right.
    {{{0, 1, 1, 3}, {-1, -1, -1, -1}}},
    // Case 14 (1110): TR, BL, BR negative. Connect Top, Left.
    {{{0, 1, 0, 2}, {-1, -1, -1, -1}}},
    // Case 15 (1111): All negative
    {{{-1, -1, -1, -1}, {-1, -1, -1, -1}}}
}};

// Determine sign of a value
static int get_sign(float v) {
    return v < 0.0f ? -1 : 1;
}

// Create a circle mesh
Mesh create_disk_mesh(float radius, int segments) {
    Mesh mesh;
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        mesh.vertices.push_back({radius * std::cos(angle), radius * std::sin(angle)});
    }
    for (int i = 0; i < segments; ++i) {
        mesh.edges.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>((i + 1) % segments)});
    }
    return mesh;
}

// Convert an implicit SDF to a mesh using marching squares
Mesh implicit_to_mesh(Scalar implicit, int resolution) {
    VM vm(implicit);
    std::deque<Tile> tiles;
    vm.evaluate(tiles, {0, 0, resolution - 1, resolution - 1});

    // Collect sign-change data and unique expressions
    std::unordered_map<int, std::pair<float, int>> local_sign_change_data; // Maps grid point index to {SDF value, expression_index}
    std::vector<std::vector<Instruction>> mesh_expressions_list;           // Stores expression vectors (std::vector<Instruction>) from each tile

    const float cell_size = 2.0f / (resolution - 1);
    std::vector<std::pair<float, float>> intersections;
    std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, Hasher> edge_to_intersection;

    // First pass: compute intersections
    for (const Tile& tile : tiles) {
        const Subgrid& subgrid = tile.subgrid;
        int start_x = subgrid.px;
        int start_y = subgrid.py;
        int nx = subgrid.nx;
        int ny = subgrid.ny;
        for (int local_y = 0; local_y <= ny; ++local_y) {
            for (int local_x = 0; local_x <= nx; ++local_x) {
                int x = start_x + local_x;
                int y = start_y + local_y;
                int i00 = y * resolution + x;
                int i01 = y * resolution + (x + 1);
                int i10 = (y + 1) * resolution + x;
                float v00 = tile.values[local_y * (nx + 1) + local_x];
                int s00 = get_sign(v00);
                // Check right edge
                if (local_x < nx) {
                    float v01 = tile.values[local_y * (nx + 1) + (local_x + 1)];
                    if (s00 * get_sign(v01) < 0) {
                        float t = interpolate(v00, v01);
                        assert(t >= 0.0f && t <= 1.0f);
                        float world_x = -1.0f + (x + t) * cell_size;
                        float world_y = -1.0f + y * cell_size;
                        uint32_t id = intersections.size();
                        intersections.push_back({world_x, world_y});
                        edge_to_intersection[{i00, i01}] = id;
                    }
                }
                // Check bottom edge
                if (local_y < ny) {
                    float v10 = tile.values[(local_y + 1) * (nx + 1) + local_x];
                    if (s00 * get_sign(v10) < 0) {
                        float t = interpolate(v00, v10);
                        assert(t >= 0.0f && t <= 1.0f);
                        float world_x = -1.0f + x * cell_size;
                        float world_y = -1.0f + (y + t) * cell_size;
                        uint32_t id = intersections.size();
                        intersections.push_back({world_x, world_y});
                        edge_to_intersection[{i00, i10}] = id;
                    }
                }
            }
        }
    }

    // Second pass: connect intersections into edges
    Mesh mesh;
    mesh.vertices = intersections;

    for (const Tile& tile : tiles) {
        // Add the tile's expression to the list and get its index.
        // Assumes Tile struct has 'instructions' (std::vector<Instruction>).
        mesh_expressions_list.push_back(tile.instructions);
        int current_expression_index = mesh_expressions_list.size() - 1;

        const Subgrid& subgrid = tile.subgrid;
        int start_x = subgrid.px;
        int start_y = subgrid.py;
        int nx = subgrid.nx;
        int ny = subgrid.ny;
        for (int local_y = 0; local_y < ny; ++local_y) {
            for (int local_x = 0; local_x < nx; ++local_x) {
                int x = start_x + local_x;
                int y = start_y + local_y;
                int i00 = y * resolution + x;
                int i01 = y * resolution + (x + 1);
                int i10 = (y + 1) * resolution + x;
                int i11 = (y + 1) * resolution + (x + 1);
                int cell[4] = {i00, i01, i10, i11};
                float vs[4] = {
                    tile.values[local_y * (nx + 1) + local_x],
                    tile.values[local_y * (nx + 1) + (local_x + 1)],
                    tile.values[(local_y + 1) * (nx + 1) + local_x],
                    tile.values[(local_y + 1) * (nx + 1) + (local_x + 1)]
                };
                int config = 0;
                if (vs[0] < 0) config |= 1;
                if (vs[1] < 0) config |= 2;
                if (vs[2] < 0) config |= 4;
                if (vs[3] < 0) config |= 8;
                if (config == 0 || config == 15) continue;
                
                int grid_point_global_indices[4] = {i00, i01, i10, i11};

                for (int k_vert = 0; k_vert < 4; ++k_vert) {
                    // Store the SDF value (vs[k_vert]) and the expression index for the global grid point.
                    // The expression_index is common for all points processed from the current tile.
                    local_sign_change_data[grid_point_global_indices[k_vert]] = {vs[k_vert], current_expression_index};
                }

                const auto& edges = marching_squares_table[config];
                for (const EdgeIndices& edge : edges) {
                    if (edge.i1 == -1) continue;
                    assert(cell[edge.j1] > cell[edge.i1]);
                    assert(cell[edge.j2] > cell[edge.i2]);
                    auto it1 = edge_to_intersection.find({cell[edge.i1], cell[edge.j1]});
                    auto it2 = edge_to_intersection.find({cell[edge.i2], cell[edge.j2]});
                    assert(it1 != edge_to_intersection.end() && it2 != edge_to_intersection.end());
                    mesh.edges.push_back({it1->second, it2->second});
                }
            }
        }
    }

    // Move sign-change data and expressions into mesh
    // Assumes Mesh struct has been updated in marching_squares.h:
    // - std::unordered_map<int, std::pair<float, int>> sign_change_data;
    // - std::vector<std::vector<Instruction>> expressions_list;
    mesh.sign_change_data = std::move(local_sign_change_data);
    mesh.expressions_list = std::move(mesh_expressions_list);
    return mesh;
} 