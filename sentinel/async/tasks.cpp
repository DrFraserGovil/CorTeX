#include "tasks.h"

Task::Task()
{
    Type = Instruction::None;
}

 Task Task::ParameterChange(std::vector<std::string_view>& input)
{
    Task T;
    T.Type = Instruction::ParameterUpdate;
    T.TaskData = input.size() > 1
            ? std::vector<std::string>(input.begin() + 1, input.end())
            : std::vector<std::string>{};
    return T;
}

Task Task::Shutdown()
{
    Task T;
    T.Type = Instruction::Shutdown;    
    return T;
}
Task Task::FileChange()
{
    Task T;
    T.Type = Instruction::FileChange;    
    return T;
}
