#include "network.h"
WikiNode::WikiNode()
{Initialised = false;}

void WikiNode::Add(std::filesystem::path path)
{
    if (!Initialised)
    {
        Name = lowerStem(path);
        Initialised = true;
    }
    Pages.push_back(std::make_unique<Page>(path));
}


WikiNetwork::WikiNetwork(std::vector<std::filesystem::path> &paths)
{
    Populate(paths);
}
void WikiNetwork::Populate(std::vector<std::filesystem::path> &paths)
{
    for (auto & path : paths)
    {
        auto stem = lowerStem(path);
        Nodes[stem].Add(path);
    }
}