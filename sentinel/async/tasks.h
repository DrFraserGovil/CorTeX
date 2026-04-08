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
    CompileAll,
    CompileRequest,
    Shutdown,
    Clean,
    None,
};

class Task
{
    public:
        Instruction Type;

        // std::variant<std::vector<fs::path>,std::vector<std::string>> TaskData;
        std::vector<std::string> TaskData;
        Task(Instruction cmd = Instruction::None);
        Task(Instruction cmd,std::vector<std::string_view> & input,int ignoreIndex=1);
        ~Task(){};
};