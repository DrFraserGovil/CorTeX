#pragma once
#include <deque>
#include "../note_helpers.h"
#include "stateStack.h"
#include "../linkSet.h"
#include "../fileHeader.h"
#include <fstream>
class TexGenerator
{
    public:
        TexGenerator(LinkSet & links);

        void BeginBuild(std::string_view preamble, BufferObj & inputBuffer,FileHeader header);
        void Flush(std::fstream & output, int truncation);
        size_t size();
        void Report();
    private:
        BufferObj Buffer; //make a copy (or take ownership) so can be modified
        LinkSet & Links; //bind as reference as immutable
        StateStack State;
        std::vector<std::string> InsertedLines;
        std::deque<int> Lines;

        void WritePreamble(std::string_view preamble,FileHeader header);
        void BodyPass();
        void LinkRender();
        template <class T> //template to handle string_views easily
        void InsertLine(T & line)
        {
            InsertedLines.push_back((std::string)line);
            Lines.push_back(-InsertedLines.size()); //we use negative numbers to index the inserted lines. The off-by-one error is to avoid -0 = 0.
        }
        void ListCheck(int i);
        void FormatCheck(int i);
        size_t ListDepth;
        std::string ListType;
        std::stack<std::string> OpenLists;
        std::map<std::string,int> ReplaceMap;
        std::map<std::string,std::tuple<std::string,std::string,std::string>> FormatReplacers;
        void TrackReplacement(std::string & cmd);
};