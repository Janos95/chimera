#include "shapes.h"
#include <cmath>
#include <cstring>
#include <imgui.h>

int Rect::rect_count = 0;
int Disk::disk_count = 0;

Mesh Rect::get_mesh() {
    Mesh mesh;
    
    // Calculate rectangle corners
    float half_width = width * 0.5f;
    float half_height = height * 0.5f;
    
    // Add vertices (bottom-left, bottom-right, top-right, top-left)
    mesh.vertices.emplace_back(pos_x - half_width, pos_y - half_height); // 0: bottom-left
    mesh.vertices.emplace_back(pos_x + half_width, pos_y - half_height); // 1: bottom-right
    mesh.vertices.emplace_back(pos_x + half_width, pos_y + half_height); // 2: top-right
    mesh.vertices.emplace_back(pos_x - half_width, pos_y + half_height); // 3: top-left
    
    // Add edges forming the rectangle
    mesh.edges.emplace_back(0, 1); // bottom edge
    mesh.edges.emplace_back(1, 2); // right edge
    mesh.edges.emplace_back(2, 3); // top edge
    mesh.edges.emplace_back(3, 0); // left edge
    
    return mesh;
}

Mesh Disk::get_mesh() {
    Mesh mesh;
    
    const int segments = 32; // Number of segments to approximate the circle
    
    // Add vertices around the circumference
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        float x = pos_x + radius * std::cos(angle);
        float y = pos_y + radius * std::sin(angle);
        mesh.vertices.emplace_back(x, y);
    }
    
    // Add edges connecting consecutive vertices
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        mesh.edges.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(next));
    }
    
    return mesh;
}

bool Rect::render_ui_properties() {
    bool changed = false;
    
    // Rectangle-specific properties
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("X", &pos_x, -2.0f, 2.0f)) changed = true;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Y", &pos_y, -2.0f, 2.0f)) changed = true;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Width", &width, 0.1f, 2.0f)) changed = true;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Height", &height, 0.1f, 2.0f)) changed = true;
    
    return changed;
}

bool Disk::render_ui_properties() {
    bool changed = false;
    
    // Disk-specific properties
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("X", &pos_x, -2.0f, 2.0f)) changed = true;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Y", &pos_y, -2.0f, 2.0f)) changed = true;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Radius", &radius, 0.05f, 1.0f)) changed = true;
    
    return changed;
}

Scalar Rect::get_sdf() const {
    Scalar sdf = rectangle(Scalar(pos_x), Scalar(pos_y), Scalar(width), Scalar(height));
    sdf.set_shape(this);
    return sdf;
}

Scalar Disk::get_sdf() const {
    Scalar sdf = disk(Scalar(pos_x), Scalar(pos_y), Scalar(radius));
    sdf.set_shape(this);
    return sdf;
} 