#include "worker_functions.h"
#include "../../global.h"
#include "../watcher.h"
#include "../../index/glob.h"

bool fileChange()
{
    auto reports = Cortex.Watcher->GetReports();
    bool dirSweepDone = false;
    for (auto & report: reports)
    {
        LOG(DEBUG) << "Processing change to " << report.Path << " in " << report.Parent.lock()->Path.Source;
        auto testPath = Cortex.Values.SourceRoot / report.Path;
        if (fs::is_directory(testPath) && !dirSweepDone)
        {
            LOG(DEBUG) << "Detected directory-level change: initialising full resweep";
            Cortex.Index.RootDir->Walk();
            dirSweepDone = true;// ensure we only do this once per report - its a clean slate wipe
        }
        if (fs::is_regular_file(testPath))
        {
            auto note = Cortex.Index.GetNote(report.Path);
            if (note.use_count() > 0)
            {
                note.lock()->Scan(true);
            }
            else
            {
                bool goodFile = !glob(testPath,Cortex.Settings.Files.IgnoredPatterns) && glob(testPath,Cortex.Settings.Files.WatchedPatterns);

                if (goodFile)
                {
                    note = Cortex.Index.NewNote(testPath,report.Parent);
                }
                else
                {
                    continue;
                }
            }

            LOG(DEBUG) << "\t" << note.lock()->Header.Title << " " << note.lock()->ID;


        }
    }
    return true;
}