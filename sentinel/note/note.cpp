#include "note.h"
#include "../global.h"

Note::Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent) : Path(path),ID(id) 
{
    
}

void Note::DiskCheck()
{
    HasBeenDeleted = false;
    if (!fs::exists(Path.Source))
    {
        HasBeenDeleted = true;
        return;
    }

    bool fileExists = fs::exists(Path.Compile);
    if (!fileExists)
    {
        IsDirty = true;
        return;
    }


    auto sourceTime = fs::last_write_time(Path.Source);
    auto compileTime = fs::last_write_time(Path.Compile);

    if (sourceTime > compileTime)
    {
        IsDirty = true;
    }

}
