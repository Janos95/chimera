#pragma once

#include <unordered_map>

struct IShape;

enum class NodeType { Add, Sub, Mul, Max, Min, Neg, Abs, Square, Sqrt, X, Y, Constant };

struct Node {
    NodeType type;
    int left_child = -1;
    int right_child = -1;
    int handle_count = 0;   // Number of Node handles referring to this node
    int ref_count = 0;      // Number of other nodes referring to this node
    float value = 0.0f; 
    const IShape* shape = nullptr;
};

class NodeManager {
public:
    constexpr static int VAR_X = 0;
    constexpr static int VAR_Y = 1;

    static NodeManager& get();

    std::unordered_map<int, Node> node_data;

    int create_node(NodeType type, int left_child = -1, int right_child = -1);

private:
    NodeManager();
};

struct Scalar {
    int index = -1;

    Scalar() = default;
    Scalar(NodeType type, int left_child = -1, int right_child = -1);
    ~Scalar();

    // Implicit conversion from float
    Scalar(float value);

    // Copy constructor and assignment
    Scalar(const Scalar& other);
    Scalar& operator=(const Scalar& other);

    // Move constructor and assignment
    Scalar(Scalar&& other) noexcept;
    Scalar& operator=(Scalar&& other) noexcept;

    Scalar operator+(const Scalar& other) const;
    Scalar operator-(const Scalar& other) const;
    Scalar operator*(const Scalar& other) const;

    Scalar operator-() const;

    Scalar square() const;
    Scalar sqrt() const;
};

Scalar max(const Scalar& a, const Scalar& b);
Scalar min(const Scalar& a, const Scalar& b);
Scalar abs(const Scalar& a);

Scalar varX();
Scalar varY();

Scalar disk(const Scalar& centerX, const Scalar& centerY, const Scalar& radius);
Scalar rectangle(const Scalar& centerX, const Scalar& centerY, const Scalar& width, const Scalar& height);

Scalar smooth_union(const Scalar& a, const Scalar& b, const Scalar& r);
