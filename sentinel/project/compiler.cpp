#include "compiler.h"
#include <sstream>
#include "JSL/modules/FileIO/FileIO.h"
#include "../global.h"
#include "../resources/resources.h"
void CompilerObject::MakePreamble()
{
    CheckResources();
    std::ostringstream preamble_head;
    std::ostringstream preamble_tail;
    preamble_head << "\\documentclass[width =" << Cortex.Settings.Document.Width << "cm, " <<  Cortex.Settings.Document.FontSize << "pt]{";
    preamble_tail << "}\n";

    bool includeHyperref = false;
    for (auto & package: Cortex.Settings.Compiler.Packages)
    {
        if (package == "hyperref")
        {
            includeHyperref = true;
        }
        else
        {
            preamble_tail << "\\usepackage{" << package << "}\n";
        }
    }

    //compile the global settings
    preamble_tail << "\\def\\titleFontSize{" << Cortex.Settings.Document.TitleSize << "}\n";
    preamble_tail << "\\def\\titleCentered{" << (int)Cortex.Settings.Document.TitleCentered << "}\n";

    if (includeHyperref)
    {
        preamble_tail << "\\usepackage{hyperref}\\hypersetup{colorlinks=true,linkcolor=blue,filecolor=blue,urlcolor=cyan}\n";
    }

    //now insert the macros file
    JSL::forLineIn(Cortex.Values.MacroFile,[&](std::string_view line)
    {
        if (line[0] != '%') // omit comment lines
        {
            preamble_tail << line << "\n";
        }
    });
    PreambleHead = preamble_head.str();
    PreambleTail = preamble_tail.str();
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
    Cortex.Index.UpdateLinkNetwork(forceAll);

    if (Cortex.Settings.System.Pause) return;
    
    

    MakePreamble();

    auto & Registry = Cortex.Index.Registry;
    auto & DirtyFiles = Cortex.Index.DirtyFiles;



    //the link netwrok update marks all files as dirty if forceAll is true, so we don't need to do anything special here, just compile everything in the dirty list until its empty
    while (DirtyFiles.size() > 0)
    {
        int fileID = DirtyFiles[0];
        if (Registry.contains(fileID))
        {
            CompileFile(Registry[fileID]);
        }
        else
        {
            LOG(DEBUG)  << "Ignoring " << DirtyFiles[0] << " due to file deletion";
        }
        DirtyFiles.pop_front();
    }
    

}

void CompilerObject::CompileFile(std::shared_ptr<Note> note)
{
    int truncation = 0;
    if (note->Buffer.Body.size() == 0)
    {
        LOG(WARN) << note->Path.Source.string() << " reached compiler without being scanned";
    }
    int fileSize = note->Buffer.Body.size();
    auto dirPath =  note->Parent.lock()->Path.Build.string();

    auto rel = fs::relative(Cortex.Values.ClassFile_Compiler,dirPath);    
    rel.replace_extension("");
    std::string preamble = PreambleHead + rel.string() + PreambleTail;

    int errorLine = -1;
    auto buildpdf = note->Path.Build;
    buildpdf.replace_extension(".pdf");
    while (truncation == 0)
    {
        note->Build(preamble,truncation);

        std::string cmd = Cortex.Settings.Compiler.CompilerCommand + " -interaction=nonstopmode -halt-on-error -output-directory=" +dirPath;
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

    LOG(WARN) << Cortex.Colours.CompileError << "Could not compile " << note->Path.Source << ", or generate a MCD";
    note->Buffer.Reset();
}

void CompilerObject::MoveSuccessful(std::shared_ptr<Note> note,fs::path pdfpath, int errorLine)
{
    
    fs::rename(pdfpath,note->Path.Compile);
    if (errorLine != -1)
    {
       LOG(WARN) << Cortex.Colours.CompileError<< "Compiler Error: " << note->Path.Source.string() << "\n\tCould not compile further than line " << errorLine; 
    }
    else
    {
        LOG(INFO) << Cortex.Colours.CompileSuccess << "Successfully compiled " << note->Header.Title << " (Note " << note->ID <<")";
    }
    note->Buffer.Reset();
}
