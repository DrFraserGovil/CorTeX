#include "stateStack.h"
#include "../../global.h"

Environment::Environment(EnvironmentDirection dir, std::string name, int line, int idx)
{
    Direction = dir;
    Name = name;
    Line = line;
    Column = idx;
    Paired = false;
}

void Environment::Pair(Environment & candidate)
{
    Paired = true;
    PairLine = candidate.Line;
    PairCol = candidate.Column;
    candidate.Paired = true;
    candidate.PairLine = Line;
    candidate.PairCol = Column;
}
bool StateStack::Pop(std::string name, int idx)
{
    Environment end(End,name,LineNoCache,idx);
    //traverse in reverse, on the assumption that the corresponding begin was most likely added recently
    for (auto it = FullStack.rbegin(); it != FullStack.rend(); ++it)
    {
        if (!it->Paired)
        {
            if (it->Name == name)
            {
                it->Pair(end);
                FullStack.push_back(end);
                Stack.pop();
                if ((int)Stack.size() < FormatBlockDepth)
                {
                    CachedFormatable = true;
                    FormatBlockDepth = -1;
                    CurrentFormat.Type = FormatStatus::Partial;
                    CurrentFormat.PartialArray.emplace_back(idx,true);
                }
                return true;
            }

            //reach here only if we close an environemnt *other* than the currently open one
            SetError("Expected end-"+it->Name+", found end-" + end.Name);
            return false;
        }
    }
    //this is an error state
    SetError("end-"+end.Name+" found, but no open environments");
    return false;
}
void StateStack::Push(std::string name, int idx)
{
    FullStack.emplace_back(Begin,name,LineNoCache,idx);
    Stack.push(name);
    if (CachedFormatable &&! IsFormatable(name))
    {
        CachedFormatable = false;
        FormatBlockDepth = Stack.size(); 
        CurrentFormat.Type = FormatStatus::Partial;
        CurrentFormat.PartialArray.emplace_back(idx,false);
    }
}

struct Toggle
{
    bool EscapeStatus;
    std::string Open;
    std::string Close;
    std::string Name;
};
const std::vector<Toggle> toggles =
{
    {false,"$$","$$","display-math"}, //needs to go first for greedy matching
    {false,"$","$","inline-math"},
    {true,"(",")","inline-math"}, 
    {true,"[","]","display-math"} 
};


#define ERRORCHECK if (Error.found){LOG(WARN) << "Scanner error: " << Error.message; return false;}

bool StateStack::Scan(std::string_view line, int lineNo)
{
    if (Error.found) return false; //consume nothing else
    lineCache = line;
    LineNoCache = lineNo;
    SizeCache = line.size();
    
    //escape status resets on every new line (unlike toggles etc)
    Escaped = false;
    CachedFormatable = CachedFormatable; // manual override takes priority
    CurrentFormat.Reset(CachedFormatable);
    
    size_t i = 0;
    while (i < SizeCache)
    {
        bool returnEarly = CheckComment(i);
        //comment escape hatch
        if (returnEarly) return true;


        if (line[i] == '\\')
        {
            Escaped = !Escaped; //we need an odd number of slashes to trigger an escape
            ++i;
            continue;
        }

        int toggleSize = CheckToggle(i);
        ERRORCHECK;
        if (toggleSize > 0)
        {
            i+=toggleSize;
            Escaped = false;
            continue;
        }

        int step = CheckEnv(Begin,i) + CheckEnv(End,i);
        if (step > 0)
        {
            i += step;
            Escaped = false;
            continue;
        }

        ++i; //fallback: if nothing interesting happened, step forward and try again!
        Escaped = false; // we set escaped to false, on the assumption that if we reached here, we either broke the escape, or found anything of interest to us.
    }
    return true;
}


bool StateStack::Scan(std::vector<std::string> & input)
{
    CachedFormatable = true;
    FormatBlockDepth = -1;
    for (int i = 0; i < input.size(); ++i)
    {
        Scan(input[i],i);
        if (Error.found)
        {
            return false;
        }
        if (ManualOverride){CurrentFormat.Enabled = false;CurrentFormat.Type = CurrentFormat.Full;}
        LineStatus.push_back(CurrentFormat);
        
    }
    return true;
}

bool StateStack::CheckComment(int i)
{
    if (!Escaped && lineCache[i] == '%')
    {
        Push("comment",i);
        Pop("comment",SizeCache); //don't need to check return here: we know it's paired!

        //check for the manual override flags
        std::string copy = (std::string)lineCache;
        std::transform(copy.begin(), copy.end(), copy.begin(),
    [](unsigned char c){ return std::tolower(c); });
        
        copy.erase(std::remove_if(copy.begin(), copy.end(), [](unsigned char x) { return std::isspace(x); }), copy.end());

        if (copy == "%noformat")
        {
            ManualOverride = true;
        }
        if (copy == "%format")
        {
            ManualOverride = false;
        }
        return true;
    }
    return false;
}


bool match(const Toggle & a, EnvironmentDirection dir, std::string_view long_b, int i,bool escape)
{
    if (escape != a.EscapeStatus) return false;

    std::string_view test;
    if (dir == Begin){   test = a.Open; }
    else             {   test = a.Close;}

    return (test == long_b.substr(i,test.size())); //string_view.substr truncates to length N or size()-i, whichever is largr
}

int StateStack::CheckToggle(int i)
{
    Toggle capture;
    if (AwaitingToggle > -1)
    {
        auto & expect = toggles[AwaitingToggle];
        if (match(expect,End,lineCache,i,Escaped))
        {
            int end = i + expect.Close.size();
            Pop(expect.Name,end);
            AwaitingToggle = -1;
            return expect.Close.size();
        }
    }
    for (int idx = 0; idx < toggles.size(); ++idx)
    {
        auto & toggle = toggles[idx];
        if (match(toggle,Begin,lineCache,i,Escaped))
        {
            if (AwaitingToggle > -1)
            {
                SetError("Improper toggle detected whilst inside " + toggles[AwaitingToggle].Name + " block.");
                return 0;
            }
            AwaitingToggle = idx;
            int start = (Escaped) ? i-1 : i;
            Push(toggle.Name,start);
            return toggle.Open.size();
        }
    }
    return 0;
}

int StateStack::CheckEnv(EnvironmentDirection dir, int i)
{
    if (!Escaped) return 0;
    std::string cmd = dir == Begin ? "begin" : "end"; // these are case sensitive, and must be escaped

    auto test = lineCache.substr(i,cmd.size());

    if (test != cmd) return 0;

    //have found either \begin or \end - now need to make sure next non-whitespace character is {
    int idx = i + cmd.size();
    int open = -1;
    while (idx < SizeCache)
    {
        auto testChar = lineCache[idx];
        if (open < 0)
        {
            bool isWS = (testChar == ' ' || testChar == '\t');
            if (!isWS)
            {
                if (testChar == '{')
                {
                    idx +=1;
                    open = idx;
                    continue;
                }
                //i.e. \beginCommand is a valid thing!
                return 0;
            }

        }
        else
        {
            if (testChar == '}')
            {
                auto arg = JSL::trim(lineCache.substr(open,idx-open));
                if (arg.size() == 0)
                {
                    SetError("Empty environment command located");
                    return 0;
                }
                if (dir == Begin)
                {
                    Push((std::string)arg,i-1);
                }
                else
                {
                    Pop((std::string)arg,i+arg.size()+1+((std::string)"\\end").size());
                }
                return idx - i;
            }
        }

        ++idx;

    }


    //only reach here if we never capture the full argument, or a whitespace: i.e. an incomplete command
    SetError("Incomplete "+cmd + " found");
    return 0;
}

void StateStack::SetError(std::string msg)
{
    Error = {true,LineNoCache,msg};
}

// FormatStatus StateStack::GetFormatStatus(int line, int col)
// {
//     for (auto & env : Stack)
//     {
//         if (env.PairLine < line) continue; //environment ends before requested line 
//     }
// }


bool StateStack::IsFormatable(std::string newType)
{
    if (ProtectedCache.contains(newType))
    {
        return ProtectedCache[newType];
    }

    auto & protectors = Cortex.Values.ProtectedEnvironments;
    bool canFormat = (std::find(protectors.begin(),protectors.end(),newType) == protectors.end());
    ProtectedCache[newType] = canFormat;
    return canFormat; 
}