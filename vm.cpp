#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <vector>
#include <array>

#include "vm.h"

//#pragma omp threadprivate(thread_batch_vars, thread_interval4_vars, thread_remap4)

VM::VM(const std::vector<Instruction>& instructions) 
    : original_instructions(instructions) {

    //#pragma omp parallel
    {
        set_batch_size(MAX_TILE_SIZE);
        interval_vars.resize(instructions.size());
        remap.resize(instructions.size());
    }
}

VM::VM(const Scalar& implicit) 
    : VM(compile(implicit)) {}

std::span<float> VM::evaluate_batch(const std::vector<Instruction>& instructions, std::span<float> x_coords, std::span<float> y_coords) {

    assert(x_coords.size() == y_coords.size() && x_coords.size() <= static_cast<size_t>(batch_capacity));
    const size_t num_instructions = instructions.size();
    const size_t n = x_coords.size();
    // Each instruction result block has size batch_capacity
    const size_t stride = batch_capacity;

#define LOOP(expr) for(size_t j = 0; j < n; j++) { batch_vars[i * stride + j] = expr; }

    for(size_t i = 0; i < num_instructions; i++) {
        const Instruction& inst = instructions[i];
        switch(inst.op) {
            case OpCode::VarX:
                LOOP(x_coords[j]);
                break;
            case OpCode::VarY:
                LOOP(y_coords[j]);
                break;
            case OpCode::Const:
                LOOP(inst.constant);
                break;
            case OpCode::Add:
                LOOP(batch_vars[inst.input0 * stride + j] + batch_vars[inst.input1 * stride + j]);
                break;
            case OpCode::Sub:
                LOOP(batch_vars[inst.input0 * stride + j] - batch_vars[inst.input1 * stride + j]);
                break;
            case OpCode::Mul:
                LOOP(batch_vars[inst.input0 * stride + j] * batch_vars[inst.input1 * stride + j]);
                break;
            case OpCode::Div:
                LOOP(batch_vars[inst.input0 * stride + j] / batch_vars[inst.input1 * stride + j]);
                break;
            case OpCode::Max:
                LOOP(fmax(batch_vars[inst.input0 * stride + j], batch_vars[inst.input1 * stride + j]));
                break;
            case OpCode::Min:
                LOOP(fmin(batch_vars[inst.input0 * stride + j], batch_vars[inst.input1 * stride + j]));
                break;
            case OpCode::Neg:
                LOOP(-batch_vars[inst.input0 * stride + j]);
                break;
            case OpCode::Abs:
                LOOP(fabsf(batch_vars[inst.input0 * stride + j]));
                break;
            case OpCode::Square:
                LOOP(batch_vars[inst.input0 * stride + j] * batch_vars[inst.input0 * stride + j]);
                break;
            case OpCode::Sqrt:
                LOOP(sqrt(batch_vars[inst.input0 * stride + j]));
                break;
        }
    }
#undef LOOP

    return std::span<float>(batch_vars.data() + (num_instructions - 1) * stride, n);
}

float min2(float a, float b) { return a < b ? a : b; }
float min4(float a, float b, float c, float d) { return min2(min2(a, b), min2(c, d)); }
float max2(float a, float b) { return a > b ? a : b; }
float max4(float a, float b, float c, float d) { return max2(max2(a, b), max2(c, d)); }

Interval4 VM::evaluate_interval4(const std::vector<Instruction>& instructions, const Interval4& x, const Interval4& y) {
    const size_t num_instructions = instructions.size();
    assert(interval_vars.size() >= num_instructions);

    for(size_t i = 0; i < num_instructions; ++i) {
        const Instruction& inst = instructions[i];
        switch(inst.op) {
            case OpCode::VarX: 
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = x.lower[j];
                    interval_vars[i].upper[j] = x.upper[j];
                }
                break;
            case OpCode::VarY: 
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = y.lower[j];
                    interval_vars[i].upper[j] = y.upper[j];
                }
                break;
            case OpCode::Const: 
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = inst.constant;
                    interval_vars[i].upper[j] = inst.constant;
                }
                break;
            case OpCode::Add:
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = interval_vars[inst.input0].lower[j] + interval_vars[inst.input1].lower[j];
                    interval_vars[i].upper[j] = interval_vars[inst.input0].upper[j] + interval_vars[inst.input1].upper[j];
                }
                break;
            case OpCode::Sub:
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = interval_vars[inst.input0].lower[j] - interval_vars[inst.input1].upper[j];
                    interval_vars[i].upper[j] = interval_vars[inst.input0].upper[j] - interval_vars[inst.input1].lower[j];
                }
                break;
            case OpCode::Mul: {
                for(int j = 0; j < 4; j++) {
                    float a = interval_vars[inst.input0].lower[j], b = interval_vars[inst.input0].upper[j];
                    float c = interval_vars[inst.input1].lower[j], d = interval_vars[inst.input1].upper[j];
                    float p1 = a*c, p2 = a*d, p3 = b*c, p4 = b*d;
                    interval_vars[i].lower[j] = min4(p1, p2, p3, p4);
                    interval_vars[i].upper[j] = max4(p1, p2, p3, p4);
                }
                break;
            }
            case OpCode::Div: {
                for(int j = 0; j < 4; j++) {
                    float a = interval_vars[inst.input0].lower[j], b = interval_vars[inst.input0].upper[j];
                    float c = interval_vars[inst.input1].lower[j], d = interval_vars[inst.input1].upper[j];
                    // Handle division by zero by clamping denominator away from zero
                    if (c <= 0.0f && d >= 0.0f) {
                        interval_vars[i].lower[j] = -std::numeric_limits<float>::infinity();
                        interval_vars[i].upper[j] = std::numeric_limits<float>::infinity();
                        continue;
                    }
                    float p1 = a/c, p2 = a/d, p3 = b/c, p4 = b/d;
                    interval_vars[i].lower[j] = min4(p1, p2, p3, p4);
                    interval_vars[i].upper[j] = max4(p1, p2, p3, p4);
                }
                break;
            }
            case OpCode::Max: {
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = max2(interval_vars[inst.input0].lower[j], interval_vars[inst.input1].lower[j]);
                    interval_vars[i].upper[j] = max2(interval_vars[inst.input0].upper[j], interval_vars[inst.input1].upper[j]);
                }
                break;
            }
            case OpCode::Min: {
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = min2(interval_vars[inst.input0].lower[j], interval_vars[inst.input1].lower[j]);
                    interval_vars[i].upper[j] = min2(interval_vars[inst.input0].upper[j], interval_vars[inst.input1].upper[j]);
                }
                break;
            }
            case OpCode::Neg:
                for(int j = 0; j < 4; j++) {
                    interval_vars[i].lower[j] = -interval_vars[inst.input0].upper[j];
                    interval_vars[i].upper[j] = -interval_vars[inst.input0].lower[j];
                    if (interval_vars[i].lower[j] > interval_vars[i].upper[j]) std::swap(interval_vars[i].lower[j], interval_vars[i].upper[j]);
                }
                break;
            case OpCode::Abs:
                for(int j = 0; j < 4; j++) {
                    float l = interval_vars[inst.input0].lower[j];
                    float u = interval_vars[inst.input0].upper[j];
                    if (l >= 0.0f) {
                        interval_vars[i].lower[j] = l;
                        interval_vars[i].upper[j] = u;
                    } else if (u <= 0.0f) {
                        interval_vars[i].lower[j] = -u;
                        interval_vars[i].upper[j] = -l;
                    } else {
                        interval_vars[i].lower[j] = 0.0f;
                        interval_vars[i].upper[j] = fmaxf(-l, u);
                    }
                }
                break;
            case OpCode::Square: {
                for(int j = 0; j < 4; j++) {
                    float l = interval_vars[inst.input0].lower[j];
                    float u = interval_vars[inst.input0].upper[j];
                    float sq_l = l*l;
                    float sq_u = u*u;
                    float min_val = min2(sq_l, sq_u);
                    if (l <= 0.0f && u >= 0.0f) min_val = 0.0f;
                    interval_vars[i].lower[j] = min_val;
                    interval_vars[i].upper[j] = max2(sq_l, sq_u);
                }
                break;
            }
            case OpCode::Sqrt: {
                for(int j = 0; j < 4; j++) {
                    float a = interval_vars[inst.input0].lower[j], b = interval_vars[inst.input0].upper[j];
                    float sqrt_a = sqrt(max2(0.0f, a));
                    float sqrt_b = sqrt(max2(0.0f, b));
                    interval_vars[i].lower[j] = (b < 0.0f) ? 0.0f : sqrt_a;
                    interval_vars[i].upper[j] = (b < 0.0f) ? 0.0f : sqrt_b;
                }
                break;
            }
        }
    }

    return interval_vars[num_instructions - 1];
}

void VM::prune_instructions4(const std::vector<Instruction>& instructions, std::array<std::vector<Instruction>, 4>& compacted_instructions) {
    int remap_size = static_cast<int>(instructions.size());
    assert(static_cast<size_t>(remap_size) <= remap.size());
    assert(interval_vars.size() >= static_cast<size_t>(remap_size));

    memset(remap.data(), -1, remap_size * 4 * sizeof(int));

    // Mark the final instruction as needed 
    for(int j = 0; j < 4; j++) {
        remap[remap_size - 1][j] = 1;
    }

    // First we do a backwards pass to determine which instructions are needed
    for (int i = remap_size - 1; i >= 0; --i) {
        const Instruction& inst = instructions[i];
        for(int j = 0; j < 4; j++) {
            if(remap[i][j] == -1) continue;

            if(inst.op == OpCode::Max) {
                assert(inst.input0 < i && inst.input1 < i);
                const float i0_lower = interval_vars[inst.input0].lower[j];
                const float i0_upper = interval_vars[inst.input0].upper[j];
                const float i1_lower = interval_vars[inst.input1].lower[j];
                const float i1_upper = interval_vars[inst.input1].upper[j];

                // We "misuse" the remap array to store which input dominates the other one
                if (i0_lower >= i1_upper) { remap[inst.input0][j] = 1; remap[i][j] = 0; } // i0 dominates, mark with 0
                else if (i1_lower >= i0_upper) { remap[inst.input1][j] = 1; assert(remap[i][j] == 1); } // i1 dominates, already marked with 1
                else { remap[inst.input0][j] = 1; remap[inst.input1][j] = 1; remap[i][j] = 2; } // Overlap, mark with 2
            } else if(inst.op == OpCode::Min) {
                assert(inst.input0 < i && inst.input1 < i);
                const float i0_lower = interval_vars[inst.input0].lower[j];
                const float i0_upper = interval_vars[inst.input0].upper[j];
                const float i1_lower = interval_vars[inst.input1].lower[j];
                const float i1_upper = interval_vars[inst.input1].upper[j];

                if (i0_upper <= i1_lower) { remap[inst.input0][j] = 1; remap[i][j] = 0; } // i0 dominates, mark with 0
                else if (i1_upper <= i0_lower) { remap[inst.input1][j] = 1; assert(remap[i][j] == 1); } // i1 dominates, already marked with 1
                else { remap[inst.input0][j] = 1; remap[inst.input1][j] = 1; remap[i][j] = 2; } // Overlap, mark with 2
            } else {
                // propagate needed instructions
                if(inst.input0 != -1) remap[inst.input0][j] = 1;
                if(inst.input1 != -1) remap[inst.input1][j] = 1;
            }
        }
    }

    // Initialize the four compacted instruction streams
    for(int j = 0; j < 4; j++) {
        compacted_instructions[j].reserve(instructions.size());
    }

    // Second we do a forwards pass to compact the instructions and compute the input remapping
    for (int i = 0; i < remap_size; ++i) {
        for(int j = 0; j < 4; j++) {
            Instruction inst = instructions[i];
            if(remap[i][j] == -1) continue;

            if(inst.op == OpCode::Max || inst.op == OpCode::Min) {
                // if one of the inputs dominates the other one, we can get rid of the max/min and 
                // remap its output to the still valid remapped input
                if(remap[i][j] != 2) {
                    remap[i][j] = remap[i][j] == 0 ? remap[inst.input0][j] : remap[inst.input1][j];
                    continue;
                }
            }

            if(inst.input0 != -1) inst.input0 = remap[inst.input0][j];
            if(inst.input1 != -1) inst.input1 = remap[inst.input1][j];
            compacted_instructions[j].push_back(inst);
            remap[i][j] = compacted_instructions[j].size() - 1;
        }
    }
}

void VM::solve_region(std::deque<Tile>& tiles, Subgrid subgrid, std::vector<Instruction> instructions) 
{
    if ((subgrid.nx + 1) * (subgrid.ny + 1) <= MAX_TILE_SIZE) 
    {
        Interval ix = get_x_interval(subgrid);
        Interval iy = get_y_interval(subgrid);
        float x_range = ix.upper - ix.lower;
        float y_range = iy.upper - iy.lower;

        std::array<float, MAX_TILE_SIZE> x_coords;
        std::array<float, MAX_TILE_SIZE> y_coords;

        // Include boundary points
        const int num_x_points = subgrid.nx + 1;
        const int num_y_points = subgrid.ny + 1;

        for (int dy = 0; dy < num_y_points; ++dy) 
        {
            float y = iy.lower + dy * y_range / subgrid.ny;
            for (int dx = 0; dx < num_x_points; ++dx) 
            {
                int idx = dy * num_x_points + dx;
                x_coords[idx] = ix.lower + dx * x_range / subgrid.nx;
                y_coords[idx] = y;
            }
        }

        const size_t total_points = (size_t)num_x_points * (size_t)num_y_points;
        std::span<float> values = evaluate_batch(instructions, {x_coords.data(), total_points}, {y_coords.data(), total_points});
        tiles.emplace_back(subgrid, values, std::move(instructions));

        return;
    } 

    // Integer division `subgrid.n / 2` means that if n is odd,
    // the first half will be `(n-1)/2` and the second half will be `n - (n-1)/2 = (n+1)/2`.
    // This ensures the entire grid is covered without overlap.
    int nx_first_half = subgrid.nx / 2;
    int nx_second_half = subgrid.nx - nx_first_half;
    int ny_first_half = subgrid.ny / 2;
    int ny_second_half = subgrid.ny - ny_first_half;

    // Create subgrids for the four quadrants
    Subgrid ll = {subgrid.px, subgrid.py, nx_first_half, ny_first_half};                       // lower left
    Subgrid lr = {subgrid.px + nx_first_half, subgrid.py, nx_second_half, ny_first_half};           // lower right
    Subgrid ul = {subgrid.px, subgrid.py + ny_first_half, nx_first_half, ny_second_half};           // upper left
    Subgrid ur = {subgrid.px + nx_first_half, subgrid.py + ny_first_half, nx_second_half, ny_second_half}; // upper right

    Subgrid regions[4] = {ll, lr, ul, ur};

    // Create intervals for the four quadrants
    Interval4 ix4, iy4;
    for (int i = 0; i < 4; i++) {
        Interval ix = get_x_interval(regions[i]);
        Interval iy = get_y_interval(regions[i]);
        ix4.lower[i] = ix.lower;
        ix4.upper[i] = ix.upper;
        iy4.lower[i] = iy.lower;
        iy4.upper[i] = iy.upper;
    }

    Interval4 ir4 = evaluate_interval4(instructions, ix4, iy4);
    std::array<std::vector<Instruction>, 4> compacted_instructions;
    prune_instructions4(instructions, compacted_instructions);

    for(size_t i = 0; i < 4; i++) 
    {
        float lower = ir4.lower[i];
        float upper = ir4.upper[i];
        if (upper < 0.0f)
            continue;
        if (lower > 0.0f) 
            continue;

        solve_region(tiles, regions[i], std::move(compacted_instructions[i]));
    }
}

void VM::evaluate(std::deque<Tile>& tiles, Subgrid grid) 
{
    // Store grid dimensions for interval calculations
    grid_nx = grid.nx;
    grid_ny = grid.ny;
    solve_region(tiles, grid, original_instructions);
}

float VM::evaluate(float x, float y) {
    std::span<float> results = evaluate_batch(original_instructions, {&x, 1}, {&y, 1});
    return results[0];
}