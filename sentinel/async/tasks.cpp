#include "tasks.h"

Task::Task(Instruction cmd)
{
    Type = cmd;
}
Task::Task(Instruction cmd,std::vector<std::string_view>& input, int offset)
{
    Type = cmd;
    TaskData = input.size() > 1
            ? std::vector<std::string>(input.begin() + offset, input.end())
            : std::vector<std::string>{};
}

//  Task Task::ParameterChange(std::vector<std::string_view>& input)
// {
//     Task T(Instruction::ParameterUpdate);
//     T.TaskData = input.size() > 1
//             ? std::vector<std::string>(input.begin() + 1, input.end())
//             : std::vector<std::string>{};
//     return T;
// }

