#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <cmath>

#include "node.h"
#include "vm.h"
#include "shapes.h"
#include "marching_squares.h"

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

TEST_CASE("Union of non-overlapping disk and rectangle - shape pointer verification") {
    // -----------------------------
    // 1. Define shapes far apart
    // -----------------------------
    auto rect_shape = std::make_unique<Rect>();
    rect_shape->name = "test_rectangle";
    rect_shape->pos_x = -0.5f;
    rect_shape->pos_y = 0.0f;
    rect_shape->width  = 0.3f;   // half-width = 0.15
    rect_shape->height = 0.2f;   // half-height = 0.10

    auto disk_shape = std::make_unique<Disk>();
    disk_shape->name = "test_disk";
    disk_shape->pos_x =  0.5f;
    disk_shape->pos_y =  0.0f;
    disk_shape->radius = 0.15f;

    // Ensure the shapes do not overlap (sanity check)
    float rect_right = rect_shape->pos_x + rect_shape->width  * 0.5f; // -0.5 + 0.15 = -0.35
    float disk_left  = disk_shape->pos_x - disk_shape->radius;        //  0.5 - 0.15 =  0.35
    CHECK(rect_right < disk_left); // gap of 0.70 on the x-axis

    // -----------------------------
    // 2. Build implicit union of the two shapes
    // -----------------------------
    Scalar union_sdf = min(rect_shape->get_sdf(), disk_shape->get_sdf());

    // -----------------------------
    // 3. Generate the mesh (marching squares)
    // -----------------------------
    const int resolution = 64;                 // grid points per axis
    const float cell_size = 2.0f / (resolution - 1); // spacing between neighbouring grid vertices (world units)
    ContouringResult result = implicit_to_mesh(union_sdf, resolution);

    // We need sign-change information to be non-empty for this test to be meaningful
    CHECK(!result.sign_change_data.empty());
    CHECK(!result.expressions_list.empty());

    // -----------------------------
    // 4. Determine a tolerance that guarantees disjoint influence zones
    //    along the x-axis the minimal gap is (disk_left - rect_right).
    //    Any grid vertex can deviate from the true position by at most
    //    half of the grid-cell diagonal: diag/2 where diag = sqrt(2)*cell_size.
    //    By picking tolerance smaller than (gap - diag) / 2 we are guaranteed
    //    that the expanded regions around both shapes are still disjoint.
    // -----------------------------
    const float gap_x = disk_left - rect_right;                // >= 0.70
    const float grid_diag = std::sqrt(2.0f) * cell_size;       // ~0.045 for resolution 64
    const float tolerance = (gap_x - grid_diag) * 0.5f;        // ~0.3275
    CHECK(tolerance > 0.0f);

    // Helper predicates to classify a world-space point (x,y)
    auto is_near_rect = [&](float x, float y) -> bool {
        float dx = std::abs(x - rect_shape->pos_x);
        float dy = std::abs(y - rect_shape->pos_y);
        return (dx <= rect_shape->width  * 0.5f + tolerance) &&
               (dy <= rect_shape->height * 0.5f + tolerance);
    };

    auto is_near_disk = [&](float x, float y) -> bool {
        float dx = x - disk_shape->pos_x;
        float dy = y - disk_shape->pos_y;
        float dist = std::sqrt(dx*dx + dy*dy);
        return dist <= disk_shape->radius + tolerance;
    };

    // Utility to discover the first non-null shape pointer in an instruction list
    auto discover_shape = [](const std::vector<Instruction>& instrs) -> const IShape* {
        for (const Instruction& inst : instrs) {
            if (inst.shape) return inst.shape;
        }
        return nullptr;
    };

    // -----------------------------
    // 5. Iterate over collected grid vertices and verify ownership
    // -----------------------------
    int rect_vertices = 0;
    int disk_vertices = 0;

    for (const auto& [vertex_idx, data] : result.sign_change_data) {
        const int i = vertex_idx / resolution; // row   index [0..resolution-1]
        const int j = vertex_idx % resolution; // column index [0..resolution-1]
        const float x = -1.0f + j * cell_size;
        const float y = -1.0f + i * cell_size;

        const bool near_rect = is_near_rect(x, y);
        const bool near_disk = is_near_disk(x, y);

        // Regions constructed above are disjoint by design
        CHECK(!(near_rect && near_disk));

        if (!near_rect && !near_disk) continue; // outside the verification bands

        // Fetch corresponding expression and deduce shape pointer
        int expr_idx = data.second;
        REQUIRE(expr_idx >= 0);
        REQUIRE(static_cast<size_t>(expr_idx) < result.expressions_list.size());
        const IShape* shape_ptr = discover_shape(result.expressions_list[expr_idx]);
        REQUIRE(shape_ptr != nullptr);

        if (near_rect) {
            rect_vertices++;
            CHECK(shape_ptr == rect_shape.get());
            // If near_rect is true, near_disk must be false by construction
            CHECK_FALSE(near_disk);
        }
        if (near_disk) {
            disk_vertices++;
            CHECK(shape_ptr == disk_shape.get());
            // Symmetric guarantee
            CHECK_FALSE(near_rect);
        }
    }

    // Ensure we actually tested something meaningful
    CHECK(rect_vertices > 0);
    CHECK(disk_vertices > 0);
} 