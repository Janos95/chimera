#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>

#include <unordered_map>

#include "node.h"
#include "vm.h"
#include "colormap.h"

struct Mesh 
{
    std::vector<std::pair<float, float>> vertices;
    std::vector<std::array<uint32_t, 2>> edges;
};

Mesh create_disk_mesh(float radius, int segments) {
    Mesh mesh;
    
    // Create vertices around the boundary
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        mesh.vertices.push_back({
            radius * std::cos(angle),
            radius * std::sin(angle)
        });
    }
    
    // Create edges around the boundary
    for (int i = 0; i < segments; ++i) {
        mesh.edges.push_back({
            static_cast<uint32_t>(i),
            static_cast<uint32_t>((i + 1) % segments)
        });
    }
    
    return mesh;
}

float interpolate(float v1, float v2) {
    if(std::abs(v1 - v2) < 1e-6) printf("interpolate: %f %f\n", v1, v2);
    return -v1 / (v2 - v1);
}

struct Hasher {
    size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
        return std::hash<uint64_t>{}(uint64_t(p.first) | (uint64_t(p.second) << 32));
    }
};

// Lookup table for marching squares
// Each entry represents the edges to create for a given configuration
// The configuration is determined by the signs of the four corners (v00, v01, v10, v11)
// Each edge connects the intersection point on edge (cell[i1], cell[j1]) to the one on (cell[i2], cell[j2])
struct EdgeIndices {
    int i1, j1, i2, j2;
};

const std::array<std::array<EdgeIndices, 2>, 16> marching_squares_table = {{
    // Config: TL | TR | BL | BR (0000 = all positive)
    // Edges: Top(0,1), Right(1,3), Bottom(2,3), Left(0,2) in cell indices

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

int get_sign(float v) {
    return v < 0 ? -1 : 1;
}

// Make sign_change_vertices accessible globally
std::unordered_map<int, float> sign_change_vertices;

Mesh implicit_to_mesh(Scalar implicit, int resolution) {
    
    VM vm(implicit);
    std::deque<Tile> tiles;
    vm.evaluate(tiles, {0, 0, resolution, resolution});

    const float cell_size = 2.0f / (resolution-1);

    std::vector<std::pair<float, float>> intersections;
    std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, Hasher> edge_to_intersection;

    // First pass: compute all intersections
    for (const Tile& tile : tiles) {
        const Subgrid& subgrid = tile.subgrid;
        int start_x = subgrid.px;
        int start_y = subgrid.py;
        int nx = subgrid.nx;
        int ny = subgrid.ny;
        
        // Process each cell in the tile
        for (int local_y = 0; local_y <= ny; ++local_y) {
            for (int local_x = 0; local_x <= nx; ++local_x) {
                int x = start_x + local_x;
                int y = start_y + local_y;
                
                int i00 = y * resolution + x;
                int i01 = y * resolution + (x+1);
                int i10 = (y+1) * resolution + x;

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

    // Second pass: connect intersections
    Mesh mesh;
    mesh.vertices = intersections;

    // Clear the map at the start
    sign_change_vertices.clear();

    // Iterate over all tiles again for the second pass
    for (const Tile& tile : tiles) {
        const Subgrid& subgrid = tile.subgrid;
        int start_x = subgrid.px;
        int start_y = subgrid.py;
        int nx = subgrid.nx;
        int ny = subgrid.ny;
        
        // Process each cell in the tile
        for (int local_y = 0; local_y < ny; ++local_y) {
            for (int local_x = 0; local_x < nx; ++local_x) {
                int x = start_x + local_x;
                int y = start_y + local_y;
                
                int i00 = y * resolution + x;
                int i01 = y * resolution + (x+1);
                int i10 = (y+1) * resolution + x;
                int i11 = (y+1) * resolution + (x+1);

                int cell[4] = {i00, i01, i10, i11};
                float vs[4] = {
                    tile.values[local_y * (nx + 1) + local_x],
                    tile.values[local_y * (nx + 1) + (local_x + 1)],
                    tile.values[(local_y + 1) * (nx + 1) + local_x],
                    tile.values[(local_y + 1) * (nx + 1) + (local_x + 1)]
                };

                // Determine the configuration index based on the signs of the corners
                int config = 0;
                if (vs[0] < 0) config |= 1;
                if (vs[1] < 0) config |= 2;
                if (vs[2] < 0) config |= 4;
                if (vs[3] < 0) config |= 8;

                // no sign changes, skip
                if(config == 0 || config == 15) continue;

                // Store vertices and their values in the map
                sign_change_vertices[i00] = vs[0];
                sign_change_vertices[i01] = vs[1];
                sign_change_vertices[i10] = vs[2];
                sign_change_vertices[i11] = vs[3];

                // Get the edges to create for this configuration
                const std::array<EdgeIndices, 2>& edges = marching_squares_table[config];
                
                // Create the edges
                for (const EdgeIndices& edge : edges) {
                    if (edge.i1 == -1) continue;  // Skip empty edges
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

    return mesh;
}

// Add colormap function
sf::Color get_colormap_color(float value, float min_value, float max_value) {
    // Normalize value to [0, 1] range
    float normalized = (value - min_value) / (max_value - min_value);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    
    // Map to colormap index (Viridis has 256 colors)
    int index = static_cast<int>(normalized * 255);
    index = std::clamp(index, 0, 255);
    
    // Return the corresponding color from Viridis colormap
    return sf::Color(
        Turbo[index][0],
        Turbo[index][1],
        Turbo[index][2],
        255  // Full opacity
    );
}

int main()
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    
    sf::RenderWindow window(sf::VideoMode({1024, 1024}), "Disk Mesh", sf::Style::Default, sf::State::Windowed, settings);
    window.setFramerateLimit(144);

    // Scale and center the mesh for display
    const float scale = 400.0f; // Scale factor for visualization
    const float center_x = 1024.0f / 2.0f;
    const float center_y = 1024.0f / 2.0f;

    // Variables for disk position and dragging
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    bool is_dragging = false;
    sf::Vector2f last_mouse_pos;

    // Create initial SDF and mesh
    const int resolution = 32;
    Scalar sdf = disk(pos_x, pos_y, 0.5f);
    //Scalar sdf = rectangle(pos_x, pos_y, 0.5f, 0.5f);
    Mesh mesh = implicit_to_mesh(sdf, resolution);

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                if (const auto* mouse_pressed = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse_pressed->button == sf::Mouse::Button::Left)
                    {
                        is_dragging = true;
                        last_mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    }
                }
            }
            else if (event->is<sf::Event::MouseButtonReleased>())
            {
                if (const auto* mouse_released = event->getIf<sf::Event::MouseButtonReleased>())
                {
                    if (mouse_released->button == sf::Mouse::Button::Left)
                    {
                        is_dragging = false;
                    }
                }
            }
            else if (event->is<sf::Event::MouseMoved>() && is_dragging)
            {
                sf::Vector2f current_mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                sf::Vector2f delta = current_mouse_pos - last_mouse_pos;
                
                // Convert screen coordinates to SDF space, inverting y to match mathematical space
                pos_x += delta.x / scale;
                pos_y += delta.y / scale;  // Corrected drag direction
                
                last_mouse_pos = current_mouse_pos;
                
                // Update SDF and remesh
                sdf = disk(pos_x, pos_y, 0.5f);
                //sdf = rectangle(pos_x, pos_y, 0.5f, 0.5f);
                mesh = implicit_to_mesh(sdf, resolution);
            }
        }

        window.clear(sf::Color(240, 240, 240));  // Light gray/whitish background

        // Draw the grid using the SDF resolution
        const float grid_spacing = 2.0f / (resolution - 1); // Spacing in SDF space for resolution-1 grid cells
        sf::Color grid_color(150, 150, 150, 50); // Darker gray, semi-transparent
        
        // Draw vertical grid lines
        for (int i = 0; i < resolution; ++i) {
            float x = -1.0f + i * grid_spacing;
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x + x * scale, center_y - scale), grid_color},
                sf::Vertex{sf::Vector2f(center_x + x * scale, center_y + scale), grid_color}
            };
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }
        
        // Draw horizontal grid lines
        for (int i = 0; i < resolution; ++i) {
            float y = -1.0f + i * grid_spacing;
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x - scale, center_y + y * scale), grid_color},
                sf::Vertex{sf::Vector2f(center_x + scale, center_y + y * scale), grid_color}
            };
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }

        // Draw the mesh edges with a more visible color
        for (const auto& edge : mesh.edges)
        {
            const auto& v1 = mesh.vertices[edge[0]];
            const auto& v2 = mesh.vertices[edge[1]];
            
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x + v1.first * scale, center_y + v1.second * scale), sf::Color::Blue},
                sf::Vertex{sf::Vector2f(center_x + v2.first * scale, center_y + v2.second * scale), sf::Color::Blue}
            };
            
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }

        // Draw the vertices as circles with a more visible color and slightly larger size
        sf::CircleShape vertex_shape(4.0f); // Increased radius to 4 pixels
        vertex_shape.setFillColor(sf::Color::Green);
        vertex_shape.setOutlineColor(sf::Color::Black);
        vertex_shape.setOutlineThickness(1.0f);
        vertex_shape.setOrigin(sf::Vector2f(4.0f, 4.0f)); // Center the circle on the vertex position
        
        for (const auto& vertex : mesh.vertices)
        {
            vertex_shape.setPosition(sf::Vector2f(center_x + vertex.first * scale, center_y + vertex.second * scale));
            window.draw(vertex_shape);
        }

        // Draw grid vertices with sign changes
        sf::CircleShape sign_change_vertex(3.0f); // Larger radius for sign change vertices
        sign_change_vertex.setOutlineThickness(0.0f); // Remove outline
        sign_change_vertex.setOrigin(sf::Vector2f(3.0f, 3.0f)); // Center the circle on the vertex position

        float min_value = std::numeric_limits<float>::max();
        float max_value = std::numeric_limits<float>::lowest();

        for(const auto& [vertex_idx, value] : sign_change_vertices) {
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
        }

        // Draw sign change vertices with color based on their value
        for (const auto& [vertex_idx, value] : sign_change_vertices) {
            int i = vertex_idx / resolution;
            int j = vertex_idx % resolution;
            float x = -1.0f + j * (2.0f / (resolution-1));
            float y = -1.0f + i * (2.0f / (resolution-1));
            
            // Set color based on the field value
            sign_change_vertex.setFillColor(get_colormap_color(value, min_value, max_value));
            
            // Draw the vertex
            sign_change_vertex.setPosition(sf::Vector2f(center_x + x * scale, center_y + y * scale));
            window.draw(sign_change_vertex);
        }

        window.display();
    }
    return 0;
}