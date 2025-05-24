#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

#include <vector>
#include <cmath>
#include <algorithm>

#include "node.h"
#include "colormap.h"
#include "marching_squares.h"

sf::Color get_colormap_color(float value, float min_value, float max_value) {
    float normalized = (value - min_value) / (max_value - min_value);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    
    int index = static_cast<int>(normalized * 255);
    index = std::clamp(index, 0, 255);
    
    return sf::Color(
        Turbo[index][0],
        Turbo[index][1],
        Turbo[index][2],
        255
    );
}

int visualization_mode = 0; // 0 = SDF Values, 1 = Instruction Length
Mesh mesh;

struct RectParams {
    float pos_x;
    float pos_y;
    float width;
    float height;
};

struct DiskParams {
    float pos_x;
    float pos_y;
    float radius;
};

// Global parameters for mesh generation
const int resolution = 33;
float union_smoothness = 0.1f;
RectParams rect_params;
DiskParams disk_params;

void update_mesh() {
    printf("[DEBUG] Regenerating mesh\n");
    
    Scalar rect_shape = rectangle(rect_params.pos_x, rect_params.pos_y, rect_params.width, rect_params.height);
    Scalar disk_shape = disk(disk_params.pos_x, disk_params.pos_y, disk_params.radius);
    
    Scalar sdf;
    if (union_smoothness > 0.0f) {
        printf("[DEBUG] Using smooth union with smoothness %.3f\n", union_smoothness);
        sdf = smooth_union(rect_shape, disk_shape, union_smoothness);
    } else {
        printf("[DEBUG] Using min union (sharp edges)\n");
        sdf = min(rect_shape, disk_shape);
    }
    
    mesh = implicit_to_mesh(sdf, resolution);
}

int main()
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    
    sf::RenderWindow window(sf::VideoMode({1024, 1024}), "Disk Mesh", sf::Style::Default, sf::State::Windowed, settings);
    window.setFramerateLimit(144);
    ImGui::SFML::Init(window);
    sf::Clock deltaClock;

    const float scale = 400.0f;
    const float center_x = 1024.0f / 2.0f;
    const float center_y = 1024.0f / 2.0f;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    bool is_dragging = false;
    sf::Vector2f last_mouse_pos;
    bool isDiskDragging = false;

    const float rect_width = 0.6f;
    const float rect_height = 0.4f;
    const float disk_radius = 0.2f;

    float disk_pos_x = pos_x + rect_width / 2.0f;
    float disk_pos_y = pos_y + rect_height / 2.0f;
    
    // Initial mesh generation
    rect_params = {pos_x, pos_y, rect_width, rect_height};
    disk_params = {disk_pos_x, disk_pos_y, disk_radius};
    update_mesh();

    while (window.isOpen())
    {
        if (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::KeyPressed>()) {
                if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (key_pressed->code == sf::Keyboard::Key::L) {
                        visualization_mode = (visualization_mode == 0) ? 1 : 0;
                    }
                }
            }
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                if (const auto* mouse_pressed = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse_pressed->button == sf::Mouse::Button::Left)
                    {
                        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                            float sdf_x = (mousePos.x - center_x) / scale;
                            float sdf_y = (mousePos.y - center_y) / scale;
                            float dx = sdf_x - disk_pos_x;
                            float dy = sdf_y - disk_pos_y;
                            if (std::sqrt(dx*dx + dy*dy) <= disk_radius)
                            {
                                isDiskDragging = true;
                                last_mouse_pos = mousePos;
                            }
                            else if (std::abs(sdf_x - pos_x) <= rect_width * 0.5f &&
                                     std::abs(sdf_y - pos_y) <= rect_height * 0.5f)
                            {
                                is_dragging = true;
                                last_mouse_pos = mousePos;
                            }
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
                        isDiskDragging = false;
                    }
                }
            }
            else if (event->is<sf::Event::MouseMoved>())
            {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                if (is_dragging)
                {
                    sf::Vector2f delta = mousePos - last_mouse_pos;
                    pos_x += delta.x / scale;
                    pos_y += delta.y / scale;
                    last_mouse_pos = mousePos;

                    printf("[DEBUG] Regenerating mesh due to rectangle dragging\n");
                    rect_params.pos_x = pos_x;
                    rect_params.pos_y = pos_y;
                    update_mesh();
                }
                else if (isDiskDragging)
                {
                    sf::Vector2f delta = mousePos - last_mouse_pos;
                    disk_pos_x += delta.x / scale;
                    disk_pos_y += delta.y / scale;
                    last_mouse_pos = mousePos;

                    printf("[DEBUG] Regenerating mesh due to disk dragging\n");
                    disk_params.pos_x = disk_pos_x;
                    disk_params.pos_y = disk_pos_y;
                    update_mesh();
                }

            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        
        // ImGui Controls Window
        ImGui::Begin("Controls");
        
        // Smoothness slider - automatically switches union type
        if (ImGui::SliderFloat("Union Smoothness", &union_smoothness, 0.0f, 1.0f))
        {
            // Update mesh when slider changes
            update_mesh();
        }
        
        // Display current union type
        ImGui::Text("Union Type: %s", union_smoothness > 0.0f ? "Smooth Union" : "Min Union (Sharp)");
        
        ImGui::Separator();
        
        ImGui::Text("Visualization Mode:");
        ImGui::RadioButton("SDF Values", &visualization_mode, 0);
        ImGui::RadioButton("Instruction Length", &visualization_mode, 1);
        
        ImGui::End();

        window.clear(sf::Color(240, 240, 240));

        const float grid_spacing = 2.0f / (resolution - 1);
        sf::Color grid_color(150, 150, 150, 50);
        
        for (int i = 0; i < resolution; ++i) {
            float x = -1.0f + i * grid_spacing;
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x + x * scale, center_y - scale), grid_color},
                sf::Vertex{sf::Vector2f(center_x + x * scale, center_y + scale), grid_color}
            };
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }
        
        for (int i = 0; i < resolution; ++i) {
            float y = -1.0f + i * grid_spacing;
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x - scale, center_y + y * scale), grid_color},
                sf::Vertex{sf::Vector2f(center_x + scale, center_y + y * scale), grid_color}
            };
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }

        for (const auto& edge : mesh.edges)
        {
            const auto& v1_data = mesh.vertices[edge.first];
            const auto& v2_data = mesh.vertices[edge.second];
            
            std::array<sf::Vertex, 2> line = {
                sf::Vertex{sf::Vector2f(center_x + v1_data.first * scale, center_y + v1_data.second * scale), sf::Color::Blue},
                sf::Vertex{sf::Vector2f(center_x + v2_data.first * scale, center_y + v2_data.second * scale), sf::Color::Blue}
            };
            
            window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
        }

        sf::CircleShape vertex_shape(4.0f);
        vertex_shape.setFillColor(sf::Color::Green);
        vertex_shape.setOutlineColor(sf::Color::Black);
        vertex_shape.setOutlineThickness(1.0f);
        vertex_shape.setOrigin(sf::Vector2f(4.0f, 4.0f));
        
        for (const auto& vertex : mesh.vertices)
        {
            vertex_shape.setPosition(sf::Vector2f(center_x + vertex.first * scale, center_y + vertex.second * scale));
            window.draw(vertex_shape);
        }

        sf::CircleShape sign_change_vertex(3.0f);
        sign_change_vertex.setOutlineThickness(0.0f);
        sign_change_vertex.setOrigin(sf::Vector2f(3.0f, 3.0f));

        float min_display_value = std::numeric_limits<float>::max();
        float max_display_value = std::numeric_limits<float>::lowest();

        if (!mesh.sign_change_data.empty()) {
            if (visualization_mode == 1) {
                if (!mesh.expressions_list.empty()) {
                    for(const auto& entry : mesh.sign_change_data) {
                        int expression_idx = entry.second.second;
                        if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < mesh.expressions_list.size()) {
                            float length = static_cast<float>(mesh.expressions_list[expression_idx].size());
                            min_display_value = std::min(min_display_value, length);
                            max_display_value = std::max(max_display_value, length);
                        }
                    }
                } else {
                    min_display_value = 0.0f;
                    max_display_value = 0.0f;
                }
            } else {
                for(const auto& entry : mesh.sign_change_data) {
                    min_display_value = std::min(min_display_value, entry.second.first);
                    max_display_value = std::max(max_display_value, entry.second.first);
                }
            }
        } else {
            min_display_value = 0.0f;
            max_display_value = 1.0f;
        }
        
        if (max_display_value == min_display_value && !mesh.sign_change_data.empty()) {
            max_display_value = min_display_value + 1.0f;
        } else if (max_display_value == min_display_value && mesh.sign_change_data.empty()) {
            // Keep the 0.0 to 1.0f range for empty data set defined above
        }

        // Get mouse position for hover detection
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool isHovering = false;
        float hoveredValue = 0.0f;
        std::string hoveredLabel = "";

        for (const auto& entry : mesh.sign_change_data) {
            int vertex_idx = entry.first;
            float sdf_value = entry.second.first;
            int expression_idx = entry.second.second;

            int i = vertex_idx / resolution;
            int j = vertex_idx % resolution;
            float x = -1.0f + j * (2.0f / (resolution-1));
            float y = -1.0f + i * (2.0f / (resolution-1));
            
            float current_value_to_display = 0.0f;
            if (visualization_mode == 1) {
                if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < mesh.expressions_list.size()) {
                    current_value_to_display = static_cast<float>(mesh.expressions_list[expression_idx].size());
                }
            } else {
                current_value_to_display = sdf_value;
            }
            
            sign_change_vertex.setFillColor(get_colormap_color(current_value_to_display, min_display_value, max_display_value));
            
            sf::Vector2f vertex_screen_pos(center_x + x * scale, center_y + y * scale);
            sign_change_vertex.setPosition(vertex_screen_pos);
            window.draw(sign_change_vertex);
            
            // Check if mouse is hovering over this vertex
            float dx = mousePos.x - vertex_screen_pos.x;
            float dy = mousePos.y - vertex_screen_pos.y;
            float distance = std::sqrt(dx*dx + dy*dy);
            
            if (distance <= 3.0f) { // 3.0f is the radius of sign_change_vertex
                isHovering = true;
                hoveredValue = current_value_to_display;
                if (visualization_mode == 1) {
                    hoveredLabel = "Instruction Length: ";
                } else {
                    hoveredLabel = "SDF Value: ";
                }
            }
        }
        
        // Display tooltip if hovering
        if (isHovering) {
            ImGui::BeginTooltip();
            if (visualization_mode == 1) {
                ImGui::Text("%s%d", hoveredLabel.c_str(), static_cast<int>(hoveredValue));
            } else {
                ImGui::Text("%s%.3f", hoveredLabel.c_str(), hoveredValue);
            }
            ImGui::EndTooltip();
        }

        ImGui::SFML::Render(window);

        window.display();
    }
    ImGui::SFML::Shutdown();
    return 0;
}