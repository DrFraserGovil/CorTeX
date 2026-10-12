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

