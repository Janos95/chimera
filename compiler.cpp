#include "compiler.h"
#include "node.h"
#include <vector>
#include <unordered_map>
#include <stack>
#include <cmath>
#include <assert.h>

struct NodeToProcess {
    int node_index;
    bool processed;
    int instruction_index;
};

std::vector<Instruction> compile(const Scalar& node) {
    std::vector<Instruction> instructions;
    std::unordered_map<int, int> node_to_instruction;
    std::stack<NodeToProcess> stack;
    
    // Start with the root node's index
    stack.push({node.index, false, -1});
    
    while (!stack.empty()) {
        NodeToProcess& current = stack.top();
        
        if (current.processed) {
            // We've already processed this node's children, now create its instruction
            const Node& data = NodeManager::get().node_data[current.node_index];
            Instruction inst;
            inst.shape = data.shape;
            inst.input0 = -1;
            inst.input1 = -1;
            
            switch (data.type) {
                case NodeType::X:
                    inst.op = OpCode::VarX;
                    break;
                case NodeType::Y:
                    inst.op = OpCode::VarY;
                    break;
                case NodeType::Constant:
                    inst.op = OpCode::Const;
                    inst.constant = data.value;
                    break;
                case NodeType::Add:
                    inst.op = OpCode::Add;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Sub:
                    inst.op = OpCode::Sub;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Mul:
                    inst.op = OpCode::Mul;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Div:
                    inst.op = OpCode::Div;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Max:
                    inst.op = OpCode::Max;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Min:
                    inst.op = OpCode::Min;
                    inst.input0 = node_to_instruction[data.left_child];
                    inst.input1 = node_to_instruction[data.right_child];
                    break;
                case NodeType::Neg:
                    inst.op = OpCode::Neg;
                    inst.input0 = node_to_instruction[data.left_child];
                    break;
                case NodeType::Abs:
                    inst.op = OpCode::Abs;
                    inst.input0 = node_to_instruction[data.left_child];
                    break;
                case NodeType::Square:
                    inst.op = OpCode::Square;
                    inst.input0 = node_to_instruction[data.left_child];
                    break;
                case NodeType::Sqrt:
                    inst.op = OpCode::Sqrt;
                    inst.input0 = node_to_instruction[data.left_child];
                    break;
            }
            
            current.instruction_index = instructions.size();
            instructions.push_back(inst);
            node_to_instruction[current.node_index] = current.instruction_index;
            stack.pop();
        } else {
            // First time seeing this node
            current.processed = true;
            
            // Check if we've already compiled this node
            auto it = node_to_instruction.find(current.node_index);
            if (it != node_to_instruction.end()) {
                stack.pop();
                continue;
            }
            
            // Push children indices onto stack in reverse order
            const Node& data = NodeManager::get().node_data[current.node_index];
            if (data.right_child != -1) {
                stack.push({data.right_child, false, -1});
            }
            if (data.left_child != -1) {
                stack.push({data.left_child, false, -1});
            }
        }
    }

    return instructions;
}

// Helper function to evaluate constant operations
float evaluate_constant_operation(OpCode op, float left_val, float right_val = 0.0f) {
    switch (op) {
        case OpCode::Add:
            return left_val + right_val;
        case OpCode::Sub:
            return left_val - right_val;
        case OpCode::Mul:
            return left_val * right_val;
        case OpCode::Div:
            return left_val / right_val;
        case OpCode::Max:
            return std::max(left_val, right_val);
        case OpCode::Min:
            return std::min(left_val, right_val);
        case OpCode::Neg:
            return -left_val;
        case OpCode::Abs:
            return std::abs(left_val);
        case OpCode::Square:
            return left_val * left_val;
        case OpCode::Sqrt:
            return std::sqrt(left_val);
        case OpCode::VarX:
        case OpCode::VarY:
        case OpCode::Const: {
            assert(false);
            return 0.0f;
        }
    }
    return 0.0f;
}

void optimize_instructions(std::vector<Instruction>& instructions) {
    // Constant propagation pass
    std::vector<bool> is_constant(instructions.size(), false);
    std::vector<float> constant_values(instructions.size(), 0.0f);
    
    // First pass: identify constants and propagate them
    for (size_t i = 0; i < instructions.size(); ++i) {
        Instruction& inst = instructions[i];
        
        switch (inst.op) {
            case OpCode::VarX:
            case OpCode::VarY:
                // Variables are not constants
                is_constant[i] = false;
                break;
                
            case OpCode::Const:
                // Already a constant
                is_constant[i] = true;
                constant_values[i] = inst.constant;
                break;
                
            case OpCode::Add:
            case OpCode::Sub:
            case OpCode::Mul:
            case OpCode::Div:
            case OpCode::Max:
            case OpCode::Min:
                if (inst.input0 != -1 && inst.input1 != -1 && 
                    is_constant[inst.input0] && is_constant[inst.input1]) {
                    float result = evaluate_constant_operation(inst.op, 
                        constant_values[inst.input0], constant_values[inst.input1]);
                    
                    inst.op = OpCode::Const;
                    inst.constant = result;
                    inst.input0 = -1;
                    inst.input1 = -1;
                    
                    is_constant[i] = true;
                    constant_values[i] = result;
                } else {
                    is_constant[i] = false;
                }
                break;
                
            case OpCode::Neg:
            case OpCode::Abs:
            case OpCode::Square:
            case OpCode::Sqrt:
                // Unary operations
                if (inst.input0 != -1 && is_constant[inst.input0]) {
                    // Input is constant, we can fold this operation
                    float result = evaluate_constant_operation(inst.op, 
                        constant_values[inst.input0]);
                    
                    // Convert this instruction to a constant
                    inst.op = OpCode::Const;
                    inst.constant = result;
                    inst.input0 = -1;
                    inst.input1 = -1;
                    
                    is_constant[i] = true;
                    constant_values[i] = result;
                } else {
                    is_constant[i] = false;
                }
                break;
        }
    }
    
    // Dead code elimination pass
    if (instructions.empty()) return;
    
    std::vector<bool> is_referenced(instructions.size(), false);
    
    is_referenced[instructions.size() - 1] = true;
    
    // Work backwards to mark all transitively referenced instructions
    for (int i = instructions.size() - 1; i >= 0; --i) {
        if (is_referenced[i]) {
            const Instruction& inst = instructions[i];
            if (inst.input0 != -1 && inst.input0 < (int)instructions.size()) {
                is_referenced[inst.input0] = true;
            }
            if (inst.input1 != -1 && inst.input1 < (int)instructions.size()) {
                is_referenced[inst.input1] = true;
            }
        }
    }
    
    // Create mapping from old indices to new indices
    std::vector<int> old_to_new(instructions.size(), -1);
    std::vector<Instruction> new_instructions;
    new_instructions.reserve(instructions.size());
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (is_referenced[i]) {
            old_to_new[i] = new_instructions.size();
            new_instructions.push_back(instructions[i]);
        }
    }
    
    for (Instruction& inst : new_instructions) {
        if (inst.input0 != -1) {
            inst.input0 = old_to_new[inst.input0];
        }
        if (inst.input1 != -1) {
            inst.input1 = old_to_new[inst.input1];
        }
    }
    
    instructions = std::move(new_instructions);
}
