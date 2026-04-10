#pragma once
#include <string>
#include "../note/note.h"
class CompilerObject
{
    public:
        std::string MakePreamble();
        static void CheckResources(bool expectWrite = false);
        void Run(bool forceAll);

    private:
        void CompileFile(std::shared_ptr<Note> note,std::string_view preamble);
        void MoveSuccessful(std::shared_ptr<Note> note,fs::path pdfpath, int ErrorLine);
};