#include "derived.h"
#include "validate.h"
namespace fs = std::filesystem;

DerivedSettings Global;

bool DerivedSettings::Synchronise()
{
    bool requiresRecompile = ValidateSettings();

    SourceRoot = (fs::path)Settings.Files.TargetDirectory;
    MetaRoot = SourceRoot / ".cortex";
    BuildRoot = MetaRoot / Settings.Files.BuildDirectory;
    CompileRoot = SourceRoot / Settings.Files.OutputDirectory;

    MetaHeadFile = MetaRoot / "about.dat";
    SettingsFile = MetaRoot / "settings.dat";
    return requiresRecompile;
}
