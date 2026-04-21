#pragma once
#include <deque>
#include <stack>
#include <string>
#include <vector>
#include <string_view>
#include <map>
enum EnvironmentDirection
{
    Begin,End
};


struct Environment
{
    EnvironmentDirection Direction;
    std::string Name;
    int Line;
    int Column;
    bool Paired;
    int PairLine;
    int PairCol;
    Environment(EnvironmentDirection dir, std::string name, int line, int idx);
    void Pair(Environment & candidate);
};

struct StateError
{
    bool found;
    int line;
    std::string message;
};


struct FormatStatus
{
    enum LineType{Full,Partial};
    LineType Type;
    bool Enabled;
    std::vector<std::pair<int,bool>> PartialArray;
    void Reset(bool current){Type = Full; Enabled = current; PartialArray.clear();}
};

class StateStack
{
    public:
        bool Scan(std::vector<std::string> & lines);
        bool Scan(std::string_view line,int lineNo);
        void Push(std::string name, int idx);
        bool Pop(std::string name, int idx);

        // FosrmatStatus GetFormatStatus(int line, int col);
        std::vector<FormatStatus> LineStatus;
        
        StateError Error;
        std::deque<Environment> FullStack;
        private:
        int CheckToggle(int i);
        int CheckEnv(EnvironmentDirection dir,int i);
        bool CheckComment(int i);
        std::string_view lineCache;
        int LineNoCache;
        size_t SizeCache;
        int AwaitingToggle = -1;
        bool Escaped;
        void SetError(std::string msg);
        std::stack<std::string> Stack;
        bool CachedFormatable;
        
        int FormatBlockDepth;
        std::map<std::string, bool> ProtectedCache;
        bool IsFormatable(std::string newType);
        FormatStatus CurrentFormat;
        bool ManualOverride = false;
};