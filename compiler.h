#pragma once

#include <vector>

struct Scalar;

enum class OpCode { VarX, VarY, Const, Add, Sub, Mul, Max, Min, Neg, Abs, Square, Sqrt };

struct Instruction 
{
    float constant;
    int input0;
    int input1;
    OpCode op;
};

std::vector<Instruction> compile(const Scalar& node);