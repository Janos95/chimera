float parse_float(std::string_view sv) 
{
    char* endptr = nullptr;
    float value = std::strtof(sv.data(), &endptr);
    return value;
}

int parse_hex_var(std::string_view sv) 
{
    std::string_view hex_part = sv.substr(1);
    char* endptr = nullptr;
    errno = 0;
    long value_long = std::strtol(hex_part.data(), &endptr, 16);
    return (int)value_long;
}

void split(std::string_view line, std::vector<std::string_view>& out) 
{
    out.clear();
    size_t start = 0;
    size_t end = 0;

    while (start < line.size()) 
    {
        end = line.find(' ', start);
        if (end == std::string_view::npos) 
        {
            out.push_back(line.substr(start));
            break;
        } 
        else 
        {
            out.push_back(line.substr(start, end - start));
            start = end + 1;
        }
    }
} 

void parse_instructions(std::istream& file, std::vector<Instruction>& instructions) 
{
    std::vector<std::string_view> tokens;
    tokens.reserve(16);
    std::string line;
    line.reserve(256);
    instructions.reserve(8000);

    while (std::getline(file, line)) 
    {
        if (line.empty() || line[0] == '#') 
        {
            continue;
        }

        split(line, tokens);

        Instruction inst{};
        if (tokens[1] == "var-x")
        {
            inst.op = OpCode::VarX;
        } 
        else if (tokens[1] == "var-y") 
        {
            inst.op = OpCode::VarY;
        } 
        else if (tokens[1] == "const") 
        {
            inst.op = OpCode::Const;
            inst.constant = parse_float(tokens[2]);
        } 
        else if (tokens[1] == "add") 
        {
            inst.op = OpCode::Add;
            inst.input0 = parse_hex_var(tokens[2]);
            inst.input1 = parse_hex_var(tokens[3]);
        } 
        else if (tokens[1] == "sub") 
        {
            inst.op = OpCode::Sub;
            inst.input0 = parse_hex_var(tokens[2]);
            inst.input1 = parse_hex_var(tokens[3]);
        } 
        else if (tokens[1] == "mul") 
        {
            inst.op = OpCode::Mul;
            inst.input0 = parse_hex_var(tokens[2]);
            inst.input1 = parse_hex_var(tokens[3]);
        } 
        else if (tokens[1] == "max") 
        {
            inst.op = OpCode::Max;
            inst.input0 = parse_hex_var(tokens[2]);
            inst.input1 = parse_hex_var(tokens[3]);
        } 
        else if (tokens[1] == "min") 
        {
            inst.op = OpCode::Min;
            inst.input0 = parse_hex_var(tokens[2]);
            inst.input1 = parse_hex_var(tokens[3]);
        } 
        else if (tokens[1] == "neg") 
        {
            inst.op = OpCode::Neg;
            inst.input0 = parse_hex_var(tokens[2]);
        } 
        else if (tokens[1] == "square") 
        {
            inst.op = OpCode::Square;
            inst.input0 = parse_hex_var(tokens[2]);
        } 
        else if (tokens[1] == "sqrt") 
        {
            inst.op = OpCode::Sqrt;
            inst.input0 = parse_hex_var(tokens[2]);
        } 
        else if (tokens[1] == "abs") 
        {
            inst.op = OpCode::Abs;
            inst.input0 = parse_hex_var(tokens[2]);
        } 
        else 
        {
            fprintf(stderr, "Unknown operation: %s\n", tokens[1].data());
            continue;
        }

        instructions.push_back(inst);
    }
}

void load_instructions(const char *filename, std::vector<Instruction>& instructions) 
{
    std::ifstream file(filename);
    if (!file) 
    {
        fprintf(stderr, "Failed to open file %s\n", filename);
        exit(1);
    }
    parse_instructions(file, instructions);
}