#pragma once
#include <string>
#include "../note/note.h"
class CompilerObject
{
    public:
        void MakePreamble();
        static void CheckResources(bool expectWrite = false);
        void Run(bool forceAll);

    private:
        void CompileFile(std::shared_ptr<Note> note);
        void MoveSuccessful(std::shared_ptr<Note> note,fs::path pdfpath, int ErrorLine);
        std::string PreambleHead;
        std::string PreambleTail;
};