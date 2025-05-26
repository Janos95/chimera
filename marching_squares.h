#pragma once

#include <vector>
#include <cassert>
#include <unordered_map>

#include "node.h"
#include "vm.h"
#include "shapes.h"

struct ContouringResult {
    Mesh mesh;
    // Sign-change vertices, their SDF values, and the index of the expression list used
    std::unordered_map<int, std::pair<float, int>> sign_change_data;
    // List of instruction vectors (expressions) used for SDF evaluation
    std::vector<std::vector<Instruction>> expressions_list;
};

ContouringResult create_disk_mesh(float radius, int segments);

ContouringResult implicit_to_mesh(Scalar implicit, int resolution);
