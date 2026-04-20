#include "worker_functions.h"
#include "../../global.h"
#include "../watcher.h"

bool fileChange()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto reports = Cortex.Watcher->GetReports();
    bool dirSweepDone = false;
    for (auto & report: reports)
    {
        
        LOG(DEBUG) << "\tProcessing change to " << report.Path;
        auto testPath = Cortex.Values.SourceRoot / report.Path;
        if ((fs::is_directory(testPath) || !fs::exists(testPath) )&& !dirSweepDone)
        {
            LOG(DEBUG) << Cortex.Colours.DebugRed<< "Initialising full resweep";
            Cortex.Index.RootDir->Walk();
            dirSweepDone = true;// ensure we only do this once per report - its a clean slate wipe
        }
        if (fs::is_regular_file(testPath))
        {
            auto note = Cortex.Index.GetNote(report.Path);
            if (note.use_count() > 0)
            {
                LOG(DEBUG) << "\t\tFile " << note.lock()->ID << " marked as dirty";
                Cortex.Index.NotifyDirty(note.lock()->ID);
                note.lock()->IsDirty = true;
            }
            else
            {
                LOG(DEBUG) << "\t\tCreating a new file";
                //tries to add a new note into the index, if it meets the criteria
                report.Parent.lock()->NewNote(testPath);
            }



        }
    }
    return true;
}