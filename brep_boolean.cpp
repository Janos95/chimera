#include "brep_boolean.h"
#include "manifold/cross_section.h"

using namespace manifold;

// Helper function to convert Mesh to Manifold SimplePolygon
SimplePolygon mesh_to_polygon(const Mesh& mesh) {
    SimplePolygon polygon;
    
    // For a simple closed shape, the vertices should be in order
    // Our mesh stores vertices and edges, we need to trace the boundary
    if (mesh.vertices.empty()) {
        return polygon;
    }
    
    // Create a simple polygon from the vertices
    // Assuming the vertices form a closed boundary in order
    for (const auto& vertex : mesh.vertices) {
        polygon.push_back({vertex.first, vertex.second});
    }
    
    return polygon;
}

// Helper function to convert Manifold Polygons back to Mesh
Mesh polygons_to_mesh(const Polygons& polygons) {
    Mesh mesh;
    
    for (const auto& polygon : polygons) {
        if (polygon.empty()) continue;
        
        uint32_t vertex_offset = static_cast<uint32_t>(mesh.vertices.size());
        
        // Add vertices
        for (const auto& point : polygon) {
            mesh.vertices.emplace_back(point.x, point.y);
        }
        
        // Add edges connecting consecutive vertices in a loop
        for (size_t i = 0; i < polygon.size(); ++i) {
            uint32_t current = vertex_offset + static_cast<uint32_t>(i);
            uint32_t next = vertex_offset + static_cast<uint32_t>((i + 1) % polygon.size());
            mesh.edges.emplace_back(current, next);
        }
    }
    
    return mesh;
}

Mesh brep_union(const std::vector<std::unique_ptr<IShape>>& shapes) {
    if (shapes.empty()) {
        return Mesh{};
    }
    
    // Convert first shape to CrossSection
    Mesh first_mesh = shapes[0]->get_mesh();
    SimplePolygon first_polygon = mesh_to_polygon(first_mesh);
    CrossSection result(first_polygon);
    
    // Union with all remaining shapes
    for (size_t i = 1; i < shapes.size(); ++i) {
        Mesh shape_mesh = shapes[i]->get_mesh();
        SimplePolygon shape_polygon = mesh_to_polygon(shape_mesh);
        CrossSection shape_cross_section(shape_polygon);
        
        // Perform boolean union
        result = result + shape_cross_section;
    }
    
    // Convert result back to Mesh
    Polygons result_polygons = result.ToPolygons();
    return polygons_to_mesh(result_polygons);
} 