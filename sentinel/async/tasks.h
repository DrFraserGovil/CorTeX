#pragma once
#include <vector>
#include <string>
#include <variant>
#include "../constants.h"
enum class Instruction
{
    ParameterUpdate,
    FileChange,
    CompileRequest,
    Shutdown,
    None,
};

class Task
{
    public:
        Instruction Type;

        std::variant<std::vector<fs::path>,std::vector<std::string>> TaskData;
        Task();
        ~Task(){};
        static Task Shutdown();
};