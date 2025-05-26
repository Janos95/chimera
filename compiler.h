#pragma once

#include <vector>
#include "shapes.h"

struct Scalar;

enum class OpCode { VarX, VarY, Const, Add, Sub, Mul, Div, Max, Min, Neg, Abs, Square, Sqrt };

struct Instruction 
{
    float constant;
    int input0;
    int input1;
    OpCode op;
    const IShape* shape;
};

std::vector<Instruction> compile(const Scalar& node);

// Optimization pass that applies various optimizations including constant propagation
void optimize_instructions(std::vector<Instruction>& instructions);