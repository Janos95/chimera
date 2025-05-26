#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <array>
#include <optional>
#include <limits>
#include <cstring>
#include <string>
#include <cstdint>
#include <set>

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
int resolution = 32;
float union_radius = 0.1f;
bool use_brep_union = false; // Toggle between brep and implicit union
std::vector<std::unique_ptr<IShape>> shapes;
int selected_shape_index = -1;
int ui_selected_shape_index = -1; // For UI selection (different from drag selection)

// State for interactive dragging ------------------------------------------------
bool is_dragging = false;
sf::Vector2f last_mouse_pos;

// Generate distinct colors using a golden-angle hue rotation in HSV space
namespace {

sf::Color hsv_to_rgb(float h, float s, float v) {
    // h in [0,360), s and v in [0,1]
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r{0}, g{0}, b{0};
    if(h < 60.0f) { r = c; g = x; }
    else if(h < 120.0f) { r = x; g = c; }
    else if(h < 180.0f) { g = c; b = x; }
    else if(h < 240.0f) { g = x; b = c; }
    else if(h < 300.0f) { r = x; b = c; }
    else { r = c; b = x; }

    return sf::Color(
        static_cast<uint8_t>((r + m) * 255.0f),
        static_cast<uint8_t>((g + m) * 255.0f),
        static_cast<uint8_t>((b + m) * 255.0f)
    );
}

inline sf::Color color_for_shape(const IShape* shape) {
    if(!shape) return sf::Color::Magenta;
    // Locate the shape index inside the global vector
    std::ptrdiff_t idx = -1;
    for(std::size_t i = 0; i < shapes.size(); ++i) {
        if(shapes[i].get() == shape) { idx = static_cast<std::ptrdiff_t>(i); break; }
    }
    if(idx < 0) return sf::Color::Magenta;

    constexpr float initialHue = 42.0f;       // deg
    constexpr float goldenAngle = 137.5f;     // deg
    float hue = std::fmod(initialHue + goldenAngle * static_cast<float>(idx), 360.0f);
    return hsv_to_rgb(hue, 0.75f, 0.9f);
}

} // namespace

void update_mesh() {
    if (shapes.empty()) {
        // Create an empty mesh or a default shape
        Scalar empty_sdf = disk(Scalar(10.0f), Scalar(10.0f), Scalar(0.01f)); // Very small disk far away
        contour_result = implicit_to_mesh(empty_sdf, resolution);
        return;
    }
    
    if (use_brep_union) {
        Mesh union_mesh = brep_union(shapes);
        contour_result.mesh = union_mesh;
        contour_result.sign_change_data.clear();
        contour_result.expressions_list.clear();
    } else {
        Scalar combined_sdf = shapes[0]->get_sdf();
        
        for (size_t i = 1; i < shapes.size(); ++i) {
            Scalar current_shape = shapes[i]->get_sdf();
            
            if (union_radius > 0.0f) {
                combined_sdf = inigo_smin(combined_sdf, current_shape, Scalar(union_radius));
            } else {
                combined_sdf = min(combined_sdf, current_shape);
            }
        }
        
        contour_result = implicit_to_mesh(combined_sdf, resolution);
    }
}

// ============================================================================
// Global constants & forward declarations
// ============================================================================
constexpr unsigned int WINDOW_SIZE = 1024;
constexpr float SCALE       = 400.0f;
constexpr float CENTER_X    = WINDOW_SIZE / 2.0f;
constexpr float CENTER_Y    = WINDOW_SIZE / 2.0f;

// Forward declarations for helper functionality implemented later in this file
void create_default_scene();
bool handle_events(sf::RenderWindow& window);
void render_imgui_controls();
void draw_grid(sf::RenderWindow& window);
void draw_mesh(sf::RenderWindow& window);
void draw_visualization_data_points(sf::RenderWindow& window);

int main()
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode({WINDOW_SIZE, WINDOW_SIZE}), "Disk Mesh", sf::Style::Default, sf::State::Windowed, settings);

    window.setFramerateLimit(144);
    [[maybe_unused]] bool imgui_init_success = ImGui::SFML::Init(window);
    sf::Clock deltaClock;

    // Build initial scene (rectangle + disk)
    create_default_scene();

    // Main application loop ---------------------------------------------------
    while (window.isOpen())
    {
        if (!handle_events(window))
            break; // window requested to close

        // Update ImGui state ---------------------------------------------------
        ImGui::SFML::Update(window, deltaClock.restart());

        // Draw GUI -------------------------------------------------------------
        render_imgui_controls();

        // Draw scene -----------------------------------------------------------
        window.clear(sf::Color(240, 240, 240));
        draw_grid(window);
        draw_mesh(window);
        draw_visualization_data_points(window);

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}

// ============================================================================
// Scene setup
// ============================================================================
void create_default_scene() {
    shapes.clear();
    auto rect_shape = std::make_unique<Rect>();
    auto disk_shape  = std::make_unique<Disk>();
    shapes.push_back(std::move(rect_shape));
    shapes.push_back(std::move(disk_shape));
    update_mesh();
}

// ============================================================================
// Event handling
// ============================================================================
bool handle_events(sf::RenderWindow& window) {
    while (const std::optional event = window.pollEvent()) {
        ImGui::SFML::ProcessEvent(window, *event);

        if (event->is<sf::Event::Closed>()) {
            window.close();
            return false;
        } else if (event->is<sf::Event::KeyPressed>()) {
            if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed>()) {
                if (key_pressed->code == sf::Keyboard::Key::L) {
                    visualization_mode = (visualization_mode == 0) ? 1 : 0;
                }
            }
        } else if (event->is<sf::Event::MouseButtonPressed>()) {
            if (const auto* mouse_pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse_pressed->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    float sdf_x = (mousePos.x - CENTER_X) / SCALE;
                    float sdf_y = (mousePos.y - CENTER_Y) / SCALE;
                    
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
        } else if (event->is<sf::Event::MouseButtonReleased>()) {
            if (const auto* mouse_released = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse_released->button == sf::Mouse::Button::Left)
                {
                    is_dragging = false;
                    selected_shape_index = -1;
                }
            }
        } else if (event->is<sf::Event::MouseMoved>()) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (is_dragging && selected_shape_index >= 0 && selected_shape_index < static_cast<int>(shapes.size())) {
                sf::Vector2f delta = mousePos - last_mouse_pos;
                float delta_x = delta.x / SCALE;
                float delta_y = delta.y / SCALE;
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
    return true;
}

// ============================================================================
// ImGui controls window
// ============================================================================
void render_imgui_controls() {
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

    // Resolution input
    if (ImGui::InputInt("Resolution", &resolution)) {
        resolution = std::max(4, std::min(resolution, 256));
        update_mesh();
    }
    ImGui::Text("Current resolution: %dx%d grid", resolution, resolution);

    ImGui::Separator();

    if (!use_brep_union) {
        if (ImGui::SliderFloat("Union Radius", &union_radius, 0.0f, 1.0f)) {
            update_mesh();
        }
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

    for (size_t i = 0; i < shapes.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        bool is_selected = (ui_selected_shape_index == static_cast<int>(i));
        std::string shape_label = shapes[i]->name;
        if (ImGui::Selectable(shape_label.c_str(), is_selected)) {
            ui_selected_shape_index = static_cast<int>(i);
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    if (ui_selected_shape_index >= 0 && ui_selected_shape_index < static_cast<int>(shapes.size())) {
        IShape* shape = shapes[ui_selected_shape_index].get();
        ImGui::SetNextItemWidth(150.0f);
        char name_buffer[256] = {0};
        strncpy(name_buffer, shape->name.c_str(), sizeof(name_buffer) - 1);
        if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
            shape->name = name_buffer;
            update_mesh();
        }

        ImGui::Separator();

        bool shape_changed = shape->render_ui_properties();
        if (shape_changed) {
            update_mesh();
        }

        ImGui::Separator();

        if (ImGui::Button("Remove Selected Shape")) {
            shapes.erase(shapes.begin() + ui_selected_shape_index);
            ui_selected_shape_index = -1;
            update_mesh();
        }
    } else if (shapes.empty()) {
        ImGui::Text("No shapes. Add some shapes to see the mesh!");
    } else {
        ImGui::Text("Nothing Selected");
    }

    ImGui::End();
}

// ============================================================================
// Rendering helpers
// ============================================================================
void draw_grid(sf::RenderWindow& window) {
    const float grid_spacing = 2.0f / (resolution - 1);
    sf::Color grid_color(150, 150, 150, 50);

    for (int i = 0; i < resolution; ++i) {
        float x = -1.0f + i * grid_spacing;
        std::array<sf::Vertex, 2> line = {
            sf::Vertex{sf::Vector2f(CENTER_X + x * SCALE, CENTER_Y - SCALE), grid_color},
            sf::Vertex{sf::Vector2f(CENTER_X + x * SCALE, CENTER_Y + SCALE), grid_color}
        };
        window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
    }

    for (int i = 0; i < resolution; ++i) {
        float y = -1.0f + i * grid_spacing;
        std::array<sf::Vertex, 2> line = {
            sf::Vertex{sf::Vector2f(CENTER_X - SCALE, CENTER_Y + y * SCALE), grid_color},
            sf::Vertex{sf::Vector2f(CENTER_X + SCALE, CENTER_Y + y * SCALE), grid_color}
        };
        window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
    }
}

void draw_mesh(sf::RenderWindow& window) {
    for (const auto& edge : mesh.edges) {
        const auto& v1_data = mesh.vertices[edge.first];
        const auto& v2_data = mesh.vertices[edge.second];
        std::array<sf::Vertex, 2> line = {
            sf::Vertex{sf::Vector2f(CENTER_X + v1_data.first * SCALE, CENTER_Y + v1_data.second * SCALE), sf::Color::Blue},
            sf::Vertex{sf::Vector2f(CENTER_X + v2_data.first * SCALE, CENTER_Y + v2_data.second * SCALE), sf::Color::Blue}
        };
        window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
    }

    sf::CircleShape vertex_shape(4.0f);
    vertex_shape.setFillColor(sf::Color::Green);
    vertex_shape.setOutlineColor(sf::Color::Black);
    vertex_shape.setOutlineThickness(1.0f);
    vertex_shape.setOrigin(sf::Vector2f(4.0f, 4.0f));

    for (const auto& vertex : mesh.vertices) {
        vertex_shape.setPosition(sf::Vector2f(CENTER_X + vertex.first * SCALE, CENTER_Y + vertex.second * SCALE));
        window.draw(vertex_shape);
    }
}

void draw_visualization_data_points(sf::RenderWindow& window) {
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
    }

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool isHovering = false;
    float hoveredValue = 0.0f;
    std::string hoveredLabel = "";

    for (const auto& entry : contour_result.sign_change_data) {
        int vertex_idx = entry.first;
        float sdf_value = entry.second.first;
        int expression_idx = entry.second.second;
        assert(expression_idx >= 0 && expression_idx < static_cast<int>(contour_result.expressions_list.size()));

        int i = vertex_idx / resolution;
        int j = vertex_idx % resolution;
        float x = -1.0f + j * (2.0f / (resolution-1));
        float y = -1.0f + i * (2.0f / (resolution-1));

        float current_value_to_display = 0.0f;
        if (visualization_mode == 1) {
            current_value_to_display = static_cast<float>(contour_result.expressions_list[expression_idx].size());
        } else if (visualization_mode == 2) {
            const IShape* shape = nullptr;
            const auto& insts = contour_result.expressions_list[expression_idx];
            if(!insts.empty()) shape = insts.back().shape;
            if (shape) {
                sign_change_vertex.setFillColor(color_for_shape(shape));
            } else {
                sign_change_vertex.setFillColor(sf::Color::Black);
            }
        } else {
            current_value_to_display = sdf_value;
            sign_change_vertex.setFillColor(get_colormap_color(current_value_to_display, min_display_value, max_display_value));
        }

        sf::Vector2f vertex_screen_pos(CENTER_X + x * SCALE, CENTER_Y + y * SCALE);
        sign_change_vertex.setPosition(vertex_screen_pos);
        window.draw(sign_change_vertex);

        float dx = mousePos.x - vertex_screen_pos.x;
        float dy = mousePos.y - vertex_screen_pos.y;
        float distance = std::sqrt(dx*dx + dy*dy);
        if (distance <= 3.0f) {
            isHovering = true;
            if (visualization_mode == 1) {
                hoveredValue = current_value_to_display;
                hoveredLabel = "Instruction Length: ";
            } else if (visualization_mode == 2) {
                const IShape* shape = nullptr;
                const auto& insts = contour_result.expressions_list[expression_idx];
                if(!insts.empty()) shape = insts.back().shape;
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
}

// ============================================================================
// End of helper implementations
// ============================================================================