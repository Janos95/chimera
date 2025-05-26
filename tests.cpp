#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <cmath>

#include "node.h"
#include "vm.h"
#include "shapes.h"

using namespace doctest;

TEST_CASE("Disk primitive") {
    std::vector<Instruction> program;
    {
        // Create a disk centered at (0,0) with radius 1
        Scalar disk_node = disk(0.0f, 0.0f, 1.0f);
        program = compile(disk_node);
    }
    CHECK(NodeManager::get().node_data.size() == 2); // Only X and Y nodes should remain
    VM vm(program);

    // Test points inside the disk
    CHECK(vm.evaluate(0.0f, 0.0f) == -1.0f);
    CHECK(vm.evaluate(0.5f, 0.5f) == Approx(sqrt(0.5f*0.5f + 0.5f*0.5f) - 1.0f));
    CHECK(vm.evaluate(0.99f, 0.0f) == Approx(0.99f - 1.0f));

    // Test points outside the disk
    CHECK(vm.evaluate(1.1f, 0.0f) == Approx(1.1f - 1.0f));
    CHECK(vm.evaluate(2.0f, 2.0f) == Approx(sqrt(2.0f*2.0f + 2.0f*2.0f) - 1.0f));
}

TEST_CASE("Rectangle primitive") {
    std::vector<Instruction> program;
    {
        // Create a rectangle centered at (0,0) with width 2 and height 1
        Scalar rect_node = rectangle(0.0f, 0.0f, 2.0f, 1.0f);
        program = compile(rect_node);
    }
    CHECK(NodeManager::get().node_data.size() == 2); // Only X and Y nodes should remain
    VM vm(program);

    // Test points inside the rectangle
    CHECK(vm.evaluate(0.0f, 0.0f) == -0.5f);
    CHECK(vm.evaluate(0.5f, 0.2f) == Approx(-0.3f));
    CHECK(vm.evaluate(0.99f, 0.49f) == Approx(-0.01f));

    // Test points outside the rectangle
    CHECK(vm.evaluate(1.1f, 0.0f) == Approx(0.1f));
    CHECK(vm.evaluate(0.0f, 0.6f) == Approx(0.1f));
    CHECK(vm.evaluate(2.0f, 2.0f) == Approx(sqrtf(3.25f)));
}

TEST_CASE("Translated disk") {
    std::vector<Instruction> program;
    {
        // Create a disk centered at (1,1) with radius 0.5
        Scalar disk_node = disk(1.0f, 1.0f, 0.5f);
        program = compile(disk_node);
    }
    CHECK(NodeManager::get().node_data.size() == 2); // Only X and Y nodes should remain
    VM vm(program);

    // Test points inside the translated disk
    CHECK(vm.evaluate(1.0f, 1.0f) == -0.5f);
    CHECK(vm.evaluate(1.2f, 1.2f) == Approx(sqrt(0.2f*0.2f + 0.2f*0.2f) - 0.5f));
    CHECK(vm.evaluate(1.49f, 1.0f) == Approx(0.49f - 0.5f));

    // Test points outside the translated disk
    CHECK(vm.evaluate(1.6f, 1.0f) == Approx(0.6f - 0.5f));
    CHECK(vm.evaluate(0.0f, 0.0f) == Approx(sqrt(1.0f*1.0f + 1.0f*1.0f) - 0.5f));
}

TEST_CASE("Translated rectangle") {
    std::vector<Instruction> program;
    {
        // Create a rectangle centered at (2,2) with width 1 and height 0.5
        Scalar rect_node = rectangle(2.0f, 2.0f, 1.0f, 0.5f);
        program = compile(rect_node);
    }
    CHECK(NodeManager::get().node_data.size() == 2); // Only X and Y nodes should remain
    VM vm(program);

    // Test points inside the translated rectangle
    CHECK(vm.evaluate(2.0f, 2.0f) == -0.25f);
    CHECK(vm.evaluate(2.2f, 2.1f) == Approx(-0.15f));
    CHECK(vm.evaluate(2.49f, 2.24f) == Approx(-0.01f));

    // Test points outside the translated rectangle
    CHECK(vm.evaluate(2.6f, 2.0f) == Approx(0.1f));
    CHECK(vm.evaluate(2.0f, 2.3f) == Approx(0.05f));
    CHECK(vm.evaluate(0.0f, 0.0f) == Approx(sqrtf(5.3125f)));
} 

TEST_CASE("Assignment") {
    Scalar rect_node = rectangle(2.0f, 2.0f, 1.0f, 0.5f);
    int n = NodeManager::get().node_data.size();
    rect_node = rectangle(2.0f, 2.0f, 1.0f, 0.5f);
    CHECK(NodeManager::get().node_data.size() == n);
    rect_node = rectangle(2.0f, 2.0f, 1.0f, 0.5f);
    CHECK(NodeManager::get().node_data.size() == n);
} 

struct TestShape : public IShape {
    Mesh get_mesh() override { return Mesh{}; }
    bool render_ui_properties() override { return false; }
    Scalar get_sdf() const override { return Scalar(1.0f); }
};

TEST_CASE("Shape pointer propagation") {
    Scalar a = varX() - 0.1f;
    auto shape = std::make_unique<TestShape>();
    a.set_shape(shape.get());

    std::vector<Instruction> instructions = compile(a);
    VM vm(instructions);
    CHECK(vm.evaluate(0.0f, 0.0f) == -0.1f);

    std::deque<Tile> tiles;
    vm.evaluate(tiles, Subgrid(0, 0, 16, 16));
    CHECK_EQ(tiles.size(), 2);

    for(const auto& tile : tiles) {
       CHECK_EQ(tile.instructions.back().shape, shape.get());
    }
}

TEST_CASE("Shape pointer propagation with min") {
    // Domain is [-1,1]x[-1,1]. 
    // We construct the union of two strips of size 0.8. 
    Scalar a = varX() + 0.2f;
    Scalar b = varY() + 0.2f;
    auto shape_a= std::make_unique<TestShape>();
    auto shape_b = std::make_unique<TestShape>();

    a.set_shape(shape_a.get());
    b.set_shape(shape_b.get());

    Scalar c = min(a,b);

    std::vector<Instruction> instructions = compile(c);
    VM vm(instructions);

    std::deque<Tile> tiles;
    vm.evaluate(tiles, Subgrid(0, 0, 16, 16));

    // We should have 3 tiles, one at the top left, bottom left and bottom right. 
    CHECK_EQ(tiles.size(), 3);

    auto find_tile = [&](Interval ix0, Interval iy0) {
        return std::find_if(tiles.begin(), tiles.end(), [&](const Tile& tile) {
            Interval ix = vm.get_x_interval(tile.subgrid);
            Interval iy = vm.get_y_interval(tile.subgrid);
            return ix.lower == ix0.lower && ix.upper == ix0.upper && iy.lower == iy0.lower && iy.upper == iy0.upper;
        });
    };

    Interval neg{-1.0f, 0.0f};
    Interval pos{0.0f, 1.0f}; 

    auto top_left_tile = find_tile(neg, pos);
    auto bottom_right_tile = find_tile(pos, neg);

    CHECK(top_left_tile != tiles.end());
    CHECK(bottom_right_tile != tiles.end());

    CHECK_EQ(top_left_tile->instructions.back().shape, shape_a.get());
    CHECK_EQ(bottom_right_tile->instructions.back().shape, shape_b.get());

    auto bottom_left_tile = find_tile(neg, neg);

    CHECK(bottom_left_tile != tiles.end());
    CHECK_EQ(bottom_left_tile->instructions.back().op, OpCode::Min);
    CHECK(!bottom_left_tile->instructions.back().shape);
}

TEST_CASE("Shape pointer propagation with smooth min") {
    // Domain is [-1,1]x[-1,1]. 
    // We construct the union of two strips of size 0.5. 
    Scalar a = varX() + 0.5f;
    Scalar b = varY() + 0.5f;
    auto shape_a= std::make_unique<TestShape>();
    auto shape_b = std::make_unique<TestShape>();

    a.set_shape(shape_a.get());
    b.set_shape(shape_b.get());

    Scalar c = inigo_smin(a,b, 0.1f);

    std::vector<Instruction> instructions = compile(c);
    VM vm(instructions);

    std::deque<Tile> tiles;
    vm.evaluate(tiles, Subgrid(0, 0, 16, 16));
}

TEST_CASE("Constant propagation - pure constants") {
    // Create an expression with constants: (2.0 + 3.0) * 4.0
    // This should be optimized to just 20.0
    Scalar a = Scalar(2.0f);
    Scalar b = Scalar(3.0f);
    Scalar c = Scalar(4.0f);
    Scalar result = (a + b) * c;
    
    // Compile to instructions
    std::vector<Instruction> instructions = compile(result);
    
    // Before optimization, should have multiple instructions
    CHECK(instructions.size() > 1);
    size_t original_size = instructions.size();
    
    // Apply optimization
    optimize_instructions(instructions);
    
    // After optimization, should have fewer instructions (optimized to single constant)
    CHECK(instructions.size() < original_size);
    CHECK(instructions.size() == 1);  // Should be optimized to just one constant instruction
    
    // After optimization, the final instruction should be a constant with value 20.0
    CHECK(!instructions.empty());
    const auto& final_inst = instructions.back();
    CHECK(final_inst.op == OpCode::Const);
    CHECK(std::abs(final_inst.constant - 20.0f) < 1e-6f);
}

TEST_CASE("Constant propagation - mixed expression") {
    // Create an expression: x + (2.0 * 3.0) - should optimize to x + 6.0
    Scalar x = varX();
    Scalar a = Scalar(2.0f);
    Scalar b = Scalar(3.0f);
    Scalar result = x + (a * b);
    
    std::vector<Instruction> instructions = compile(result);
    size_t original_size = instructions.size();
    
    // Apply optimization
    optimize_instructions(instructions);
    
    // After optimization, should have fewer instructions (constants folded)
    CHECK(instructions.size() < original_size);
    CHECK(instructions.size() == 3);  // Should be: VarX, Const(6.0), Add
    
    // Should have: VarX, Const(6.0), Add
    bool found_var_x = false;
    bool found_const_6 = false;
    bool found_add = false;
    
    for (const auto& inst : instructions) {
        if (inst.op == OpCode::VarX) found_var_x = true;
        if (inst.op == OpCode::Const && std::abs(inst.constant - 6.0f) < 1e-6f) found_const_6 = true;
        if (inst.op == OpCode::Add) found_add = true;
    }
    
    CHECK(found_var_x);
    CHECK(found_const_6);
    CHECK(found_add);
}

TEST_CASE("Constant propagation - unary operations") {
    // Create an expression: sqrt(square(3.0)) - should optimize to 3.0
    Scalar a = Scalar(3.0f);
    Scalar result = a.square().sqrt();
    
    std::vector<Instruction> instructions = compile(result);
    size_t original_size = instructions.size();
    
    // Apply optimization
    optimize_instructions(instructions);
    
    // After optimization, should have fewer instructions (optimized to single constant)
    CHECK(instructions.size() < original_size);
    CHECK(instructions.size() == 1);  // Should be optimized to just one constant instruction
    
    // Should optimize to a single constant
    CHECK(!instructions.empty());
    const auto& final_inst = instructions.back();
    CHECK(final_inst.op == OpCode::Const);
    CHECK(std::abs(final_inst.constant - 3.0f) < 1e-6f);
}

TEST_CASE("Constant propagation - no optimization needed") {
    // Create an expression with only variables: x + y
    Scalar x = varX();
    Scalar y = varY();
    Scalar result = x + y;
    
    std::vector<Instruction> instructions = compile(result);
    size_t original_size = instructions.size();
    
    // Apply optimization
    optimize_instructions(instructions);
    
    // Should not change the number of instructions since no constants to fold
    CHECK(instructions.size() == original_size);
    
    // Should still have VarX, VarY, and Add
    bool found_var_x = false;
    bool found_var_y = false;
    bool found_add = false;
    
    for (const auto& inst : instructions) {
        if (inst.op == OpCode::VarX) found_var_x = true;
        if (inst.op == OpCode::VarY) found_var_y = true;
        if (inst.op == OpCode::Add) found_add = true;
    }
    
    CHECK(found_var_x);
    CHECK(found_var_y);
    CHECK(found_add);
}