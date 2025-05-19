#include "compiler.h"
#include "node.h"
#include <vector>
#include <unordered_map>
#include <stack>

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
