#include "directory_display.h"
#include "JSL/modules/Display/Log.h"
#include "JSL/modules/Strings/Strings.h"
#include "../project/index.h"
#include "../project/metadata.h"
#include "../constants.h"

std::vector<JSL::detail::ColourConstructor> colorArray = {
    txt::Colour(255, 99, 132),   // red
    txt::Colour(54, 162, 235),   // blue
    txt::Colour(255, 205, 86),   // yellow
    txt::Colour(75, 192, 192),   // teal
    txt::Colour(153, 102, 255),  // purple
    txt::Colour(100, 255, 180),  // mint
    txt::Colour(255, 140, 200),  // pink
    txt::Colour(180, 220, 255),  // light blue
    txt::Colour(255, 159, 64),   // orange
    txt::Colour(120, 255, 120)   // green
};

int ColIndex;

void display(std::weak_ptr<Directory> input,std::string dirPrefix, std::string filePrefix)
{
    auto dir = input.lock();

    auto dirCol = colorArray[ColIndex % colorArray.size()];
    ++ColIndex;

    std::string col(dirCol.buf,dirCol.len);
    std::string newFilePrefix = filePrefix + col + "│   ";    
    std::string endFilePrefix = filePrefix + col + "    ";    
    auto & children = dir->Children;
    if (children.size() == 0)
    {
        newFilePrefix = endFilePrefix;
    }
    if (dir->IsRoot)
    {
        LOG(INFO) << dirCol << txt::Bold << MainProject.Name << txt::Italics<< " (Project root)";
    }
    else
    {
        LOG(INFO) << dirPrefix  << txt::Bold  << dirCol << dir->FullPath.filename().string();
    }
    auto & notes = dir->Notes;
    for (auto note : notes)
    {
        LOG(INFO) << newFilePrefix << txt::Italics << note.first.filename().string() << " " << note.second.lock()->UniqueID;
    }
    //child directories
    size_t i = 0;
    for (auto & child : children)
    {
        std::string junction = col +"├───";
        if (i == children.size()-1)
        {
            junction = col + "└───";
            newFilePrefix = endFilePrefix;
        }
        display(child.second,filePrefix + junction,newFilePrefix);
        ++i;
    }
}


void directoryDisplay(std::vector<std::string_view> array)
{
    auto s = MasterIndex.GetStructure();
    ColIndex = 0;
    if (array.size() > 1)
    {
        s = s.lock()->Find(JSL::split(array[1],"/"));
    }

    display(s,"","");
}