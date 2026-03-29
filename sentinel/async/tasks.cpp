#include "tasks.h"

Task::Task()
{
    Type = Instruction::None;
}

Task Task::Shutdown()
{
    Task T;
    T.Type = Instruction::Shutdown;
    
    return T;
}
