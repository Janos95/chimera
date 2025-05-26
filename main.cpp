#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

#include "node.h"
#include "colormap.h"
#include "marching_squares.h"
#include "shapes.h"
#include "brep_boolean.h"

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

int visualization_mode = 0; // 0 = SDF Values, 1 = Instruction Length, 2 = Shape
ContouringResult contour_result;
Mesh& mesh = contour_result.mesh; // Reference for easy access

// Global parameters for mesh generation
const int resolution = 32;
float union_radius = 0.1f;
bool use_brep_union = false; // Toggle between brep and implicit union
std::vector<std::unique_ptr<IShape>> shapes;
int selected_shape_index = -1;
int ui_selected_shape_index = -1; // For UI selection (different from drag selection)

void update_mesh() {
    // Clear any previously created nodes so we start from a clean slate
    //NodeManager::get().reset();
    printf("[DEBUG] Regenerating mesh with %zu shapes\n", shapes.size());
    
    if (shapes.empty()) {
        // Create an empty mesh or a default shape
        Scalar empty_sdf = disk(Scalar(10.0f), Scalar(10.0f), Scalar(0.01f)); // Very small disk far away
        contour_result = implicit_to_mesh(empty_sdf, resolution);
        return;
    }
    
    if (use_brep_union) {
        printf("[DEBUG] Using explicit (BREP) boolean operations\n");
        
        // Use the proper brep_union function
        Mesh union_mesh = brep_union(shapes);
        
        // Create a ContouringResult from the union mesh
        contour_result.mesh = union_mesh;
        contour_result.sign_change_data.clear();
        contour_result.expressions_list.clear();
    } else {
        printf("[DEBUG] Using implicit boolean operations\n");
        
        // Use implicit boolean operations (SDF-based)
        Scalar combined_sdf = shapes[0]->get_sdf();
        
        for (size_t i = 1; i < shapes.size(); ++i) {
            Scalar current_shape = shapes[i]->get_sdf();
            
            if (union_radius > 0.0f) {
                combined_sdf = inigo_smin(combined_sdf, current_shape, Scalar(union_radius));
            } else {
                printf("[DEBUG] Using min union (sharp edges)\n");
                combined_sdf = min(combined_sdf, current_shape);
            }
        }
        
        contour_result = implicit_to_mesh(combined_sdf, resolution);
    }
}

int main()
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    
    sf::RenderWindow window(sf::VideoMode({1024, 1024}), "Disk Mesh", sf::Style::Default, sf::State::Windowed, settings);
    window.setFramerateLimit(144);
    [[maybe_unused]] bool imgui_init_success = ImGui::SFML::Init(window);
    sf::Clock deltaClock;

    const float scale = 400.0f;
    const float center_x = 1024.0f / 2.0f;
    const float center_y = 1024.0f / 2.0f;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    bool is_dragging = false;
    sf::Vector2f last_mouse_pos;

    const float rect_width = 0.6f;
    const float rect_height = 0.4f;
    const float disk_radius = 0.2f;

    float disk_pos_x = pos_x + rect_width / 2.0f;
    float disk_pos_y = pos_y + rect_height / 2.0f;
    
    // Initial mesh generation
    auto rect_shape = std::make_unique<Rect>();
    rect_shape->name = "rectangle0";
    rect_shape->pos_x = pos_x;
    rect_shape->pos_y = pos_y;
    rect_shape->width = rect_width;
    rect_shape->height = rect_height;
    
    auto disk_shape = std::make_unique<Disk>();
    disk_shape->name = "disk1";
    disk_shape->pos_x = disk_pos_x;
    disk_shape->pos_y = disk_pos_y;
    disk_shape->radius = disk_radius;
    
    shapes.push_back(std::move(rect_shape));
    shapes.push_back(std::move(disk_shape));
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
                        
                        selected_shape_index = -1;
                        
                        // Check if we clicked on any shape
                        for (size_t i = 0; i < shapes.size(); ++i) {
                            bool clicked = false;
                            
                            if (const Disk* disk_shape = dynamic_cast<const Disk*>(shapes[i].get())) {
                                float dx = sdf_x - disk_shape->pos_x;
                                float dy = sdf_y - disk_shape->pos_y;
                                clicked = std::sqrt(dx*dx + dy*dy) <= disk_shape->radius;
                            } else if (const Rect* rect_shape = dynamic_cast<const Rect*>(shapes[i].get())) {
                                clicked = std::abs(sdf_x - rect_shape->pos_x) <= rect_shape->width * 0.5f &&
                                         std::abs(sdf_y - rect_shape->pos_y) <= rect_shape->height * 0.5f;
                            }
                            
                            if (clicked) {
                                selected_shape_index = static_cast<int>(i);
                                is_dragging = true;
                                last_mouse_pos = mousePos;
                                break;
                            }
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
                        selected_shape_index = -1;
                    }
                }
            }
            else if (event->is<sf::Event::MouseMoved>())
            {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                if (is_dragging && selected_shape_index >= 0 && selected_shape_index < static_cast<int>(shapes.size()))
                {
                    sf::Vector2f delta = mousePos - last_mouse_pos;
                    float delta_x = delta.x / scale;
                    float delta_y = delta.y / scale;
                    last_mouse_pos = mousePos;

                    if (Disk* disk_shape = dynamic_cast<Disk*>(shapes[selected_shape_index].get())) {
                        disk_shape->pos_x += delta_x;
                        disk_shape->pos_y += delta_y;
                    } else if (Rect* rect_shape = dynamic_cast<Rect*>(shapes[selected_shape_index].get())) {
                        rect_shape->pos_x += delta_x;
                        rect_shape->pos_y += delta_y;
                    }
                    
                    update_mesh();
                }
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        
        // ImGui Controls Window
        ImGui::Begin("Controls");
        
        // Boolean operation type selection
        ImGui::Text("Boolean Operation Type:");
        if (ImGui::RadioButton("Implicit (SDF-based)", !use_brep_union)) {
            use_brep_union = false;
            update_mesh();
        }
        if (ImGui::RadioButton("Explicit (BREP-based)", use_brep_union)) {
            use_brep_union = true;
            update_mesh();
        }
        
        ImGui::Separator();
        
        // Smoothness slider - only for implicit operations
        if (!use_brep_union) {
            if (ImGui::SliderFloat("Union Radius", &union_radius, 0.0f, 1.0f))
            {
                // Update mesh when slider changes
                update_mesh();
            }
            
            // Display current union type
            ImGui::Text("Union Type: %s", union_radius > 0.0f ? "Smooth Union" : "Min Union (Sharp)");
        } else {
            ImGui::Text("Explicit boolean operations: Shape outlines combined");
        }
        
        ImGui::Separator();
        
        ImGui::Text("Visualization Mode:");
        ImGui::RadioButton("SDF Values", &visualization_mode, 0);
        ImGui::RadioButton("Instruction Length", &visualization_mode, 1);
        ImGui::RadioButton("Shape", &visualization_mode, 2);
        
        ImGui::Separator();
        
        // Shapes Management
        ImGui::Text("Shapes (%zu):", shapes.size());
        
        // Add shape buttons
        if (ImGui::Button("Add Rectangle")) {
            shapes.push_back(std::make_unique<Rect>());
            update_mesh();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Add Disk")) {
            shapes.push_back(std::make_unique<Disk>());
            update_mesh();
        }
        
        ImGui::Separator();
        
        // List all shapes
        for (size_t i = 0; i < shapes.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            
            // Create selectable item for each shape
            bool is_selected = (ui_selected_shape_index == static_cast<int>(i));
            std::string shape_label = shapes[i]->name;
            
            if (ImGui::Selectable(shape_label.c_str(), is_selected)) {
                ui_selected_shape_index = static_cast<int>(i);
            }
            
            ImGui::PopID();
        }
        
        ImGui::Separator();
        
        // Show properties for selected shape
        if (ui_selected_shape_index >= 0 && ui_selected_shape_index < static_cast<int>(shapes.size())) {
            IShape* shape = shapes[ui_selected_shape_index].get();
            
            // Common name editing for all shapes
            ImGui::SetNextItemWidth(150.0f);
            char name_buffer[256] = {0};
            strncpy(name_buffer, shape->name.c_str(), sizeof(name_buffer) - 1);
            if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
                shape->name = name_buffer;
                update_mesh();
            }
            
            ImGui::Separator();
            
            // Use the shape's own UI property editor for shape-specific properties
            bool shape_changed = shape->render_ui_properties();
            
            if (shape_changed) {
                update_mesh();
            }
            
            ImGui::Separator();
            
            // Remove button for selected shape
            if (ImGui::Button("Remove Selected Shape")) {
                shapes.erase(shapes.begin() + ui_selected_shape_index);
                ui_selected_shape_index = -1; // Clear selection
                update_mesh();
            }
        } else if (shapes.empty()) {
            ImGui::Text("No shapes. Add some shapes to see the mesh!");
        } else {
            ImGui::Text("Nothing Selected");
        }
        
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

        if (!contour_result.sign_change_data.empty()) {
            if (visualization_mode == 1) {
                if (!contour_result.expressions_list.empty()) {
                    for(const auto& entry : contour_result.sign_change_data) {
                        int expression_idx = entry.second.second;
                        if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < contour_result.expressions_list.size()) {
                            float length = static_cast<float>(contour_result.expressions_list[expression_idx].size());
                            min_display_value = std::min(min_display_value, length);
                            max_display_value = std::max(max_display_value, length);
                        }
                    }
                } else {
                    min_display_value = 0.0f;
                    max_display_value = 0.0f;
                }
            } else {
                for(const auto& entry : contour_result.sign_change_data) {
                    min_display_value = std::min(min_display_value, entry.second.first);
                    max_display_value = std::max(max_display_value, entry.second.first);
                }
            }
        } else {
            min_display_value = 0.0f;
            max_display_value = 1.0f;
        }
        
        if (max_display_value == min_display_value && !contour_result.sign_change_data.empty()) {
            max_display_value = min_display_value + 1.0f;
        } else if (max_display_value == min_display_value && contour_result.sign_change_data.empty()) {
            // Keep the 0.0 to 1.0f range for empty data set defined above
        }

        // Get mouse position for hover detection
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool isHovering = false;
        float hoveredValue = 0.0f;
        std::string hoveredLabel = "";

        for (const auto& entry : contour_result.sign_change_data) {
            int vertex_idx = entry.first;
            float sdf_value = entry.second.first;
            int expression_idx = entry.second.second;

            int i = vertex_idx / resolution;
            int j = vertex_idx % resolution;
            float x = -1.0f + j * (2.0f / (resolution-1));
            float y = -1.0f + i * (2.0f / (resolution-1));
            
            float current_value_to_display = 0.0f;
            if (visualization_mode == 1) {
                if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < contour_result.expressions_list.size()) {
                    current_value_to_display = static_cast<float>(contour_result.expressions_list[expression_idx].size());
                }
            } else if (visualization_mode == 2) {
                const IShape* shape = nullptr;
                if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < contour_result.expressions_list.size()) {
                    const auto& insts = contour_result.expressions_list[expression_idx];
                    if (!insts.empty()) {
                        shape = insts.back().shape;
                    }
                }
                if (shape) {
                    hoveredLabel = "Shape: " + shape->name;
                } else {
                    hoveredLabel = "Shape: None";
                }
            } else {
                current_value_to_display = sdf_value;
                sign_change_vertex.setFillColor(get_colormap_color(current_value_to_display, min_display_value, max_display_value));
            }
            
            sf::Vector2f vertex_screen_pos(center_x + x * scale, center_y + y * scale);
            sign_change_vertex.setPosition(vertex_screen_pos);
            window.draw(sign_change_vertex);
            
            // Check if mouse is hovering over this vertex
            float dx = mousePos.x - vertex_screen_pos.x;
            float dy = mousePos.y - vertex_screen_pos.y;
            float distance = std::sqrt(dx*dx + dy*dy);
            
            if (distance <= 3.0f) { // 3.0f is the radius of sign_change_vertex
                isHovering = true;
                if (visualization_mode == 1) {
                    hoveredValue = current_value_to_display;
                    hoveredLabel = "Instruction Length: ";
                } else if (visualization_mode == 2) {
                    const IShape* shape = nullptr;
                    if (expression_idx >= 0 && static_cast<size_t>(expression_idx) < contour_result.expressions_list.size()) {
                        const auto& insts = contour_result.expressions_list[expression_idx];
                        if (!insts.empty()) {
                            shape = insts.back().shape;
                        }
                    }
                    if (shape) {
                        hoveredLabel = "Shape: " + shape->name;
                    } else {
                        hoveredLabel = "Shape: None";
                    }
                } else {
                    hoveredValue = current_value_to_display;
                    hoveredLabel = "SDF Value: ";
                }
            }
        }
        
        // Display tooltip if hovering
        if (isHovering) {
            ImGui::BeginTooltip();
            if (visualization_mode == 1) {
                ImGui::Text("%s%d", hoveredLabel.c_str(), static_cast<int>(hoveredValue));
            } else if (visualization_mode == 2) {
                ImGui::Text("%s", hoveredLabel.c_str());
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