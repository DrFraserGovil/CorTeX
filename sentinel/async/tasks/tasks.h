#pragma once
#include <vector>
#include <string>
#include <string_view>
enum class Instruction
{
    Shutdown=0,
    SettingView=1,
    SettingChange=2,
    SettingVectorAdd=3,
    SettingVectorRemove=4,
    FileChange=5,
    Compile=6,
    IncrementalCompile=7,
    List=8,
    Reset=9,
    Clean=10,
    PauseSignal=11,
    None=12,
};

const std::vector<std::string_view> InstructionNames{
    "Shutdown",
    "View Setting",
    "Change Setting",
    "Setting Insert",
    "Setting Remove",
    "File Change",
    "Full Recompile",
    "Incremental Recompile",
    "Show Directory",
    "Reset",
    "Clean",
    "Pause"
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