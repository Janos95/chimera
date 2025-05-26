#include "node.h"
#include <vector>
#include <assert.h>
#include <cmath>


NodeManager& NodeManager::get() {
    static NodeManager instance;
    return instance;
}

NodeManager::NodeManager() {
    node_data[VAR_X] = {NodeType::X, -1, -1, 1, 0};
    node_data[VAR_Y] = {NodeType::Y, -1, -1, 1, 0};
}

int NodeManager::create_node(NodeType type, int left_child, int right_child) {
    static int next_index = VAR_Y + 1;
    int index = next_index++;
    node_data[index] = {type, left_child, right_child, 1, 0};
    
    if (left_child != -1) {
        node_data[left_child].ref_count++;
    }
    if (right_child != -1) {
        node_data[right_child].ref_count++;
    }
    
    return index;
}

Scalar::Scalar(float value) {
    index = NodeManager::get().create_node(NodeType::Constant);
    NodeManager::get().node_data[index].value = value;
}

Scalar::Scalar(NodeType type, int left_child, int right_child) {
    index = NodeManager::get().create_node(type, left_child, right_child);
}

Scalar::~Scalar() {
    if (index == -1) return;
    
    NodeManager& forest = NodeManager::get();
    Node& data = forest.node_data[index];
    
    if (--data.handle_count != 0) return;
    if (data.ref_count != 0) return;
    
    std::vector<int> stack;
    stack.push_back(index);
    
    while (!stack.empty()) {
        int current = stack.back();
        stack.pop_back();
        
        Node& current_data = forest.node_data[current];
        assert(current_data.type != NodeType::X && current_data.type != NodeType::Y);
        
        // Push children onto stack if they exist
        if (current_data.left_child != -1) {
            Node& left_data = forest.node_data[current_data.left_child];
            left_data.ref_count--;
            if (left_data.handle_count == 0 && left_data.ref_count == 0) {
                stack.push_back(current_data.left_child);
            }
        }
        
        if (current_data.right_child != -1) {
            Node& right_data = forest.node_data[current_data.right_child];
            right_data.ref_count--;
            if (right_data.handle_count == 0 && right_data.ref_count == 0) {
                stack.push_back(current_data.right_child);
            }
        }
        
        // Delete the current node
        forest.node_data.erase(current);
    }
}

void swap(Scalar& first, Scalar& second) noexcept {
    std::swap(first.index, second.index);
}

Scalar::Scalar(const Scalar& other) : index(other.index) {
    if (index != -1) {
        ++NodeManager::get().node_data[index].handle_count;
    }
}

Scalar& Scalar::operator=(const Scalar& other) {
    Scalar temp(other);
    swap(*this, temp);
    return *this;
}

Scalar::Scalar(Scalar&& other) noexcept : index(other.index) {
    other.index = -1;
}

Scalar& Scalar::operator=(Scalar&& other) noexcept {
    swap(*this, other);
    return *this;
}

Scalar Scalar::operator+(const Scalar& other) const {
    return Scalar(NodeType::Add, index, other.index);
}

Scalar Scalar::operator-(const Scalar& other) const {
    return Scalar(NodeType::Sub, index, other.index);
}

Scalar Scalar::operator*(const Scalar& other) const {
    return Scalar(NodeType::Mul, index, other.index);
}

Scalar Scalar::operator/(const Scalar& other) const {
    return Scalar(NodeType::Div, index, other.index);
}

Scalar Scalar::operator-() const {
    return Scalar(NodeType::Neg, index);
}

Scalar Scalar::square() const {
    return Scalar(NodeType::Square, index);
}

Scalar Scalar::sqrt() const {
    return Scalar(NodeType::Sqrt, index);
}

Scalar max(const Scalar& a, const Scalar& b) {
    return Scalar(NodeType::Max, a.index, b.index);
}

Scalar min(const Scalar& a, const Scalar& b) {
    return Scalar(NodeType::Min, a.index, b.index);
}

Scalar abs(const Scalar& a) {
    return Scalar(NodeType::Abs, a.index);
}

Scalar varX() {
    Scalar result;
    result.index = NodeManager::VAR_X;
    ++NodeManager::get().node_data[result.index].handle_count;
    return result;
}

Scalar varY() {
    Scalar result;
    result.index = NodeManager::VAR_Y;
    ++NodeManager::get().node_data[result.index].handle_count;
    return result;
}

Scalar disk(const Scalar& centerX, const Scalar& centerY, const Scalar& radius) {
    Scalar dx = varX() - centerX;
    Scalar dy = varY() - centerY;
    return (dx.square() + dy.square()).sqrt() - radius;
}

Scalar rectangle(const Scalar& centerX, const Scalar& centerY, const Scalar& width, const Scalar& height) {
    Scalar dx = abs(varX() - centerX) - (width * 0.5f);
    Scalar dy = abs(varY() - centerY) - (height * 0.5f);

    Scalar dist_outside = max(dx, 0.0f).square() + max(dy, 0.0f).square();
    dist_outside = dist_outside.sqrt();

    Scalar dist_inside = min(max(dx, dy), 0.0f);
    
    return dist_outside + dist_inside;
}

// max(r, min(a, b)) - sqrt(max(r-a, 0)^2 + max(r-b, 0)^2)
Scalar smooth_union(const Scalar& a, const Scalar& b, const Scalar& r) {
    Scalar val_a = r - a;
    Scalar val_b = r - b;
    Scalar zero = 0.0f;
    Scalar u_x = max(val_a, zero);
    Scalar u_y = max(val_b, zero);
    Scalar length_u_sq = u_x.square() + u_y.square();
    Scalar length_u = length_u_sq.sqrt();
    return max(r, min(a, b)) - length_u;
}

// circular
//float smin( float a, float b, float k )
//{
//    k *= 1.0/(1.0-sqrt(0.5));
//    float h = max( k-abs(a-b), 0.0 )/k;
//    return min(a,b) - k*0.5*(1.0+h-sqrt(1.0-h*(h-2.0)));
//}
Scalar inigo_smin(const Scalar& a, const Scalar& b, const Scalar& r) {
    Scalar k = r * (1.0f / (1.0f - std::sqrt(0.5f)));
    Scalar h = max(k - abs(a - b), 0.0f) / k;
    Scalar h2 = h * (h - 2.0f);
    return min(a, b) - k * 0.5f * (Scalar(1.0f) + h - (Scalar(1.0f) - h2).sqrt());
}

void Scalar::set_shape(const IShape* shape) {
    NodeManager::get().node_data[index].shape = shape;
}