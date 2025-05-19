#pragma once

#include "compiler.h"

#include <vector>
#include <span>
#include <deque>

constexpr int MAX_TILE_SIZE = 256;

// A subgrid is a rectangular set of grid vertices. It is defined by its lower left corner
// and the number of vertices in the x and y directions. Note that the subgrid includes
// the the grid points that are nx, ny units away from the lower left corner. For example,
// if px = 0, py = 0, nx = 2, and ny = 2, the subgrid includes the grid points (0,0), (0,1),
// (1,0), (1,1), (0,2), (1,2), (2,0), and (2,1).
struct Subgrid 
{
    int px, py; 
    int nx, ny;
};

struct Tile {
    Tile(Subgrid subgrid, std::span<float> values, std::vector<Instruction> instructions) : 
        subgrid(subgrid),  
        instructions(std::move(instructions)) 
    {
        memcpy(this->values, values.data(), values.size() * sizeof(float));
    }
    Subgrid subgrid;
    // values are stored in row-major order
    float values[MAX_TILE_SIZE];
    std::vector<Instruction> instructions;
};

struct Interval 
{ 
    float lower, upper; 
};

struct Interval4 
{ 
    alignas(16) float lower[4]; 
    alignas(16) float upper[4]; 
};

struct VM
{
    VM(const Scalar& implicit);
    VM(const std::vector<Instruction>& instructions);

    std::vector<Instruction> original_instructions;

    void evaluate(std::deque<Tile>& tiles, Subgrid grid);

    float evaluate(float x, float y);

    std::span<float> evaluate_batch(const std::vector<Instruction>& instructions, std::span<float> x_coords, std::span<float> y_coords);

    void set_batch_size(int size) {
        batch_capacity = size;
        batch_vars.resize(batch_capacity * original_instructions.size());
    }

    // Domain information
    const float domain_x_min = -1.0f;
    const float domain_x_max = 1.0f;
    const float domain_y_min = -1.0f;
    const float domain_y_max = 1.0f;
    int grid_nx = -1;
    int grid_ny = -1;

    // Helper to compute intervals for a subgrid
    Interval get_x_interval(const Subgrid& subgrid) const {
        float x_size = domain_x_max - domain_x_min;
        float x_step = x_size / grid_nx;
        return {
            domain_x_min + subgrid.px * x_step,
            domain_x_min + (subgrid.px + subgrid.nx) * x_step
        };
    }

    Interval get_y_interval(const Subgrid& subgrid) const {
        float y_size = domain_y_max - domain_y_min;
        float y_step = y_size / grid_ny;
        return {
            domain_y_min + subgrid.py * y_step,
            domain_y_min + (subgrid.py + subgrid.ny) * y_step
        };
    }

    void solve_region(std::deque<Tile>& tiles, Subgrid subgrid, std::vector<Instruction> instructions);

private:
    Interval4 evaluate_interval4(const std::vector<Instruction>& instructions, const Interval4& x, const Interval4& y);
    void prune_instructions4(const std::vector<Instruction>& instructions, std::array<std::vector<Instruction>, 4>& compacted_instructions);

    int batch_capacity = 0;
    std::vector<float> batch_vars;
    std::vector<Interval4> interval_vars;
    std::vector<std::array<int, 4>> remap;
};
