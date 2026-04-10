

struct TryPush
{
    template<typename T>
    static void Run(JSL::ParameterDescription& desc, const std::string& value,bool & runningSuccess)
    {
        if (runningSuccess) return;
        if (desc.Type == typeid(std::vector<T>))
        {
            auto* vec = static_cast<std::vector<T>*>(desc.RiskyPointer);

            vec->push_back(JSL::ParseTo<T>(value));

            runningSuccess = true;
        }
    }
};

struct TryRemove
{
    template<typename T>
    static void Run(JSL::ParameterDescription& desc, const std::string& value,bool & runningSuccess)
    {
        if (runningSuccess) return;
        if (desc.Type == typeid(std::vector<T>))
        {
            auto* vec = static_cast<std::vector<T>*>(desc.RiskyPointer);

            T target = JSL::ParseTo<T>(value);
            auto it = std::find(vec->begin(),vec->end(),target);
            if (it==vec->end())
            {
                LOG(ERROR) << target << " is not in target. Cannot remove";
            }

            vec->erase(it);
            runningSuccess = true;
        }
    }
};



template<class Functor>
bool ProcessVector(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return false;
    }
    std::ostringstream s(data[1]);
    for (size_t i = 2; i < data.size(); ++i)
    {
        s << " " << data[i]; 
    }
    std::string parser = s.str();
    
    try
    {
        bool hasSucceeded = false;

        Functor::template Run<int>(description,parser,hasSucceeded);
        Functor::template Run<double>(description,parser,hasSucceeded);
        Functor::template Run<std::string>(description,parser,hasSucceeded);
    
        if (!hasSucceeded)
        {
            LOG(WARN) << "Vector actions are not supported for objects of type '" << description.TypeString << "'";
        }

        return ConsolidateChanges();
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
        return false;
    }
}
