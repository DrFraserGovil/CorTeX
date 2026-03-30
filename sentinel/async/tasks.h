#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include "../constants.h"
enum class Instruction
{
    ParameterUpdate,
    VectorAdd,
    VectorRemove,
    FileChange,
    CompileRequest,
    Shutdown,
    None,
};

class Task
{
    public:
        Instruction Type;

        // std::variant<std::vector<fs::path>,std::vector<std::string>> TaskData;
        std::vector<std::string> TaskData;
        Task();
        ~Task(){};
        static Task Shutdown();
        static Task FileChange();
        static Task ParameterChange(std::vector<std::string_view> & input);
};