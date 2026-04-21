#include "compiler.h"
#include <sstream>
#include <array>
#include "JSL/modules/FileIO/FileIO.h"
#include "../global.h"
#include "../resources/resources.h"
#include "../note/builder/stateStack.h"
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

struct CompileReturn
{
    bool Success;
    std::string ErrorMessage;
};

CompileReturn ExternalCall(std::string cmd,std::shared_ptr<Note> note, std::string_view preamble, int truncation,fs::path expectedOut)
{
    note->Build(preamble,truncation);
 
    std::array<char, 256> buffer;
    std::string result;
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }

    // Read the output line by line
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }

    // pclose returns the termination status of the command
    int status = pclose(pipe);
    int exitCode = WEXITSTATUS(status);
    if (exitCode == 0 && fs::exists(expectedOut))
    {
        LOG(DEBUG) << "Success";
        return {true,""};
    }
    else
    {
        return {false,result};
    }
}

void CompilerObject::CompileFile(std::shared_ptr<Note> note)
{
    LOG(DEBUG) << "Compiling " << note->Header.Title;
    int truncation = 0;
    if (note->Buffer.Body.size() == 0)
    {
        LOG(WARN) << note->Path.Source.string() << " reached compiler without being scanned";
    }

    auto & body = note->Buffer.Body;
    auto State = StateStack();
    State.Scan(body);

    for (int i = 0; i < body.size(); ++i)
    {
        std::string_view line = body[i];
        auto state = State.LineStatus[i];
        if (state.Type==state.Full)
        {
            auto col = (state.Enabled) ? txt::Green : txt::Red;
            std::cout << col << body[i] << std::endl;
        }
        else
        {
            int prev = 0;
            bool type = !state.PartialArray[0].second;
            auto col = (type)? Cortex.Colours.DebugGreen : Cortex.Colours.DebugYellow;
            for (int i = 0; i < state.PartialArray.size(); ++i)
            {
                int idx = state.PartialArray[i].first;
                std::cout << col << line.substr(prev,idx-prev);
                prev = idx;
                col = ( state.PartialArray[i].second) ? Cortex.Colours.DebugGreen : Cortex.Colours.DebugYellow;
            }
            std::cout << col << line.substr(prev,std::string_view::npos) <<std::endl;
        }
    }


    if (State.Error.found) return;

    int fileSize = note->Buffer.Body.size();
    auto dirPath =  note->Parent.lock()->Path.Build.string();

    auto rel = fs::relative(Cortex.Values.ClassFile_Compiler,dirPath);    
    rel.replace_extension("");
    std::string preamble = PreambleHead + rel.string() + PreambleTail;

    std::string cmd = Cortex.Settings.Compiler.CompilerCommand + " -interaction=nonstopmode -halt-on-error -output-directory=" +dirPath;
    cmd += " " + note->Path.Build.string();

    int errorLine = -1;
    auto buildpdf = note->Path.Build;
    buildpdf.replace_extension(".pdf");

    while (truncation == 0)
    {
        note->Build(preamble,truncation);
        auto result = ExternalCall(cmd,note,preamble,truncation,buildpdf);
       

        if (result.Success)
        {
            MoveSuccessful(note,buildpdf,errorLine);
            return;
        } 
        else
        {
            LOG(INFO) << result.ErrorMessage;
        }
        ++truncation;
        errorLine = fileSize - truncation + note->BodyStartLine;
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


