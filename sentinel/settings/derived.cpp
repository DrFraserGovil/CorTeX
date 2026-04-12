#include "derived.h"
#include "validate.h"
namespace fs = std::filesystem;

bool DerivedSettings::Synchronise(SettingsObject & Settings,SettingsObject & CachedSettings)
{
    bool requiresRecompile = ValidateSettings(Settings,CachedSettings);

    SourceRoot = (fs::path)Settings.Files.TargetDirectory;
    MetaRoot = SourceRoot / ".cortex";
    BuildRoot = SourceRoot / Settings.Files.BuildDirectory;
    CompileRoot = SourceRoot / Settings.Files.OutputDirectory;

    MetaHeadFile = MetaRoot / "about.dat";
    SettingsFile = MetaRoot / "settings.dat";

    ClassFile_User = MetaRoot / "cortex.cls";
    ClassFile_Compiler = BuildRoot / "cortex.cls";
    MacroFile = SourceRoot/ "macros.sty";
    SharedSessionDirectory = fs::temp_directory_path() / "cortex";
    if (!fs::exists(SharedSessionDirectory)) fs::create_directories(SharedSessionDirectory);
    return requiresRecompile;
}
