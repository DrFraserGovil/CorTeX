#include "parser.h"
#include "../global.h"
#include <string_view>


inline bool equal(std::string_view a, std::vector<std::string_view> b)
{
    for (auto & elem : b)
    {
        bool test = JSL::insensitiveEquals(a,elem);
        if (test) return true;
    }
    return false;
}

using vsv = std::vector<std::string_view>;
#define IF_ARG(arg...) if(equal(cmd,(vsv){arg}))

Task CompoundCommands(std::vector<std::string_view> & array)
{
    auto cmd = array[0];

    IF_ARG("show")          return Task(Instruction::SettingView,array);
    IF_ARG("reset")         return Task(Instruction::Reset,array);
    IF_ARG("list","ls")     return Task(Instruction::List,array);
    IF_ARG("set")           return Task(Instruction::SettingChange,array);
    IF_ARG("add")           return Task(Instruction::SettingVectorAdd,array);
    IF_ARG("remove","rm")   return Task(Instruction::SettingVectorRemove,array);
    
  
    // return empty task
    return Task();
};



Task SimpleCommands(std::string_view cmd)
{
    IF_ARG("compile","make")    return Task(Instruction::Compile);
    IF_ARG("exit","shutdown")   return Task(Instruction::Shutdown);
    IF_ARG("clean")             return Task(Instruction::Clean);

    //spoof in an alias
    vsv pause{"","1"};
    vsv resume{"","0"};
    IF_ARG("pause")             return Task(Instruction::PauseSignal,pause);
    IF_ARG("resume")            return Task(Instruction::PauseSignal,resume);

    // return empty task
    return Task();
}

#define TRY(method,input) {auto test = method(input); if (test.Type != Instruction::None) return test;}

Task ParseCommand(std::string_view cmd)
{
    TRY(SimpleCommands,cmd);

    auto array = JSL::split(cmd," ");

    TRY(CompoundCommands,array);

    
    if (array.size() > 1)
    {
        std::swap(array[1],array[0]);
        TRY(CompoundCommands,array);
    }

    LOG(WARN) << "Unknown command";
    
    return Task();
}