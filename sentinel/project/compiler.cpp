#include "compiler.h"
#include <sstream>
#include "../global.h"
#include "../resources/resources.h"
std::string CompilerObject::MakePreamble()
{
    CheckResources();
    std::ostringstream preamble;
    preamble << "\\documentclass[width =" << Cortex.Settings.Document.Width << "cm, " <<  Cortex.Settings.Document.FontSize << "pt]{cortex}\n";

    bool includeHyperref = false;
    for (auto & package: Cortex.Settings.Document.Packages)
    {
        if (package == "hyperref")
        {
            includeHyperref = true;
        }
        else
        {
            preamble << "\\usepackage{" << package << "}\n";
        }
    }

    //compile the global settings
    preamble << "\\def\\titleFontSize{" << Cortex.Settings.Document.TitleSize << "}\n";
    preamble << "\\def\\titleCentered{" << (int)Cortex.Settings.Document.TitleCentered << "}\n";

    if (includeHyperref)
    {
        preamble << "\\usepackage{hyperref}\\hypersetup{colorlinks=true,linkcolor=blue,filecolor=blue,urlcolor=cyan}\n";
    }

    //now insert the macros file
    JSL::forLineIn(Cortex.Values.MacroFile,[&](std::string_view line)
    {
        if (line[0] != '%') // omit comment lines
        {
            preamble << line << "\n";
        }
    });

    return preamble.str();
}




void CompilerObject::CheckResources(bool expectWrite)
{
    if (!fs::exists(Cortex.Values.ClassFile_User))
    {
        if (!expectWrite){LOG(WARN) << ".cls File missing from metadata directory: generating a new one";}
        JSL::writeStringToFile(Cortex.Values.ClassFile_User,Resources::cortex_cls,std::ios::out);
    }
    //always move the class file to build in case the user has made local changes
    fs::copy_file(Cortex.Values.ClassFile_User,Cortex.Values.ClassFile_Compiler,fs::copy_options::overwrite_existing);
    
    if (!fs::exists(Cortex.Values.MacroFile))
    {
        if (!expectWrite){LOG(WARN) << "Macro File missing: generating a new one";}
        JSL::writeStringToFile(Cortex.Values.MacroFile,Resources::macros_sty,std::ios::out);
    }
    //no need to move this one, as it is inserted directly

}

void CompilerObject::Run(bool forceAll)
{
    if (Cortex.Settings.System.Pause) return;
    auto preamble =MakePreamble();

    auto & Registry = Cortex.Index.Registry;
    auto & DirtyFiles = Cortex.Index.DirtyFiles;

    if (forceAll)
    {
        for (auto & file : Registry)
        {
            CompileFile(file.second,preamble);
        }
    }
    else
    {
        while (DirtyFiles.size() > 0)
        {
            int fileID = DirtyFiles[0];
            if (Registry.contains(fileID))
            {
                CompileFile(Registry[fileID],preamble);
            }
            else
            {
                LOG(DEBUG) << JSL::Text::Colour(50,80,50) << "Ignoring " << DirtyFiles[0] << " due to file deletion";
            }
            DirtyFiles.pop_front();
        }
    }

}

void CompilerObject::CompileFile(std::shared_ptr<Note> note,std::string_view preamble)
{
    int truncation = 0;
    note->Scan(true);
    Cortex.Index.Aliases.Sync(note);
    int fileSize = note->Buffer.Body.size();
    auto dirPath =  note->Parent.lock()->Path.Build.string();
    int errorLine = -1;
    auto buildpdf = note->Path.Build;
    buildpdf.replace_extension(".pdf");
    while (truncation < fileSize)
    {
        note->Build(preamble,truncation);

        std::string cmd = "pdflatex -interaction=nonstopmode -halt-on-error -output-directory=" +dirPath;
        cmd += " " + note->Path.Build.string() + "> /dev/null 2>&1";
        int status = std::system(cmd.c_str());
        int exitCode = WEXITSTATUS(status);
        
        if (exitCode == 0 && fs::exists(buildpdf))
        {
            MoveSuccessful(note,buildpdf,errorLine);
            return;
        } 
        ++truncation;
        errorLine = fileSize - truncation;
    }

    LOG(WARN) << txt::Red << "Could not compile " << note->Path.Source << ", or generate a MCD";
    note->Buffer.Reset();
}

void CompilerObject::MoveSuccessful(std::shared_ptr<Note> note,fs::path pdfpath, int errorLine)
{
    
    fs::rename(pdfpath,note->Path.Compile);
    if (errorLine != -1)
    {
       LOG(WARN) << txt::Red<< "Compiler Error: " << note->Path.Source.string() << "\n\tCould not compile further than line " << errorLine; 
    }
    else
    {
        LOG(DEBUG) <<JSL::Text::Colour(50,80,50) << "Successfully compiled " << note->Header.Title << " (Note " << note->ID <<")";
    }
    note->Buffer.Reset();
}
