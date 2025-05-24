#pragma once

#include <vector>
#include <cassert>
#include <unordered_map>

#include "node.h"
#include "vm.h"

// Mesh structure holding vertices and edges from marching squares
struct Mesh {
    std::vector<std::pair<float, float>> vertices;
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    // Sign-change vertices, their SDF values, and the index of the expression list used
    std::unordered_map<int, std::pair<float, int>> sign_change_data;
    // List of instruction vectors (expressions) used for SDF evaluation
    std::vector<std::vector<Instruction>> expressions_list;
};

// Create a circular boundary mesh (used for testing/disk visualization)
Mesh create_disk_mesh(float radius, int segments);

// Convert an implicit function (SDF) to a mesh using marching squares
Mesh implicit_to_mesh(Scalar implicit, int resolution);
