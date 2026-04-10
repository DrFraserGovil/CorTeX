#include "derived.h"
#include "validate.h"
namespace fs = std::filesystem;

bool DerivedSettings::Synchronise(SettingsObject & Settings,SettingsObject & CachedSettings)
{
    bool requiresRecompile = ValidateSettings(Settings,CachedSettings);

    SourceRoot = (fs::path)Settings.Files.TargetDirectory;
    MetaRoot = SourceRoot / ".cortex";
    BuildRoot = MetaRoot / Settings.Files.BuildDirectory;
    CompileRoot = SourceRoot / Settings.Files.OutputDirectory;

    MetaHeadFile = MetaRoot / "about.dat";
    SettingsFile = MetaRoot / "settings.dat";
    return requiresRecompile;
}
