#pragma once

#include <string>
#include <vector>
#include "node.h"

struct Mesh {
    std::vector<std::pair<float, float>> vertices;
    std::vector<std::pair<uint32_t, uint32_t>> edges;
};

struct IShape {
    std::string name;

    virtual ~IShape() = default;
    virtual Mesh get_mesh() = 0;
    virtual bool render_ui_properties() = 0; // Returns true if any property was changed
    virtual Scalar get_sdf() const = 0; // Returns the SDF representation of the shape
};

struct Rect : IShape {
    static int rect_count;

    Rect(const std::string& name = "") {
        if(name.empty()) {
            this->name = "rect" + std::to_string(rect_count);
            rect_count++;
        }
    }

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float width = 0.3f;
    float height = 0.2f;

    Mesh get_mesh() override;
    bool render_ui_properties() override;
    Scalar get_sdf() const override;
};

struct Disk : IShape {
    static int disk_count;

    Disk(const std::string& name = "") {
        if(name.empty()) {
            this->name = "disk" + std::to_string(disk_count);
            disk_count++;
        }
    }
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float radius = 0.2f;

    Mesh get_mesh() override;
    bool render_ui_properties() override;
    Scalar get_sdf() const override;
};
