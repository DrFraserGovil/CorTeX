#pragma once
#include <vector>
#include <string>
#include <string_view>
enum class Instruction
{
    Shutdown,
    SettingView,
    SettingChange,
    SettingVectorAdd,
    SettingVectorRemove,
    FileChange,
    Compile,
    IncrementalCompile,
    List,
    Reset,
    Clean,
    PauseSignal,
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