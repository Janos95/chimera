#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <cmath>

#include "node.h"
#include "vm.h"

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