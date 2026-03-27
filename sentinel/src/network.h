#pragma once

#include "globalSettings.h"
#include <unordered_map>

#include <filesystem>
#include <vector>
#include "utils.h"
#include "page.h"

class WikiNetwork; //forward declaration for parent reference
class WikiNode
{
    public:
    
        WikiNode();
        
        void Add(std::filesystem::path path);
        // void ResolveLinks(const WikiNetwork & parent);
        friend class WikiNetwork;
    private:
        std::string Name;
        bool Initialised;
        std::vector<std::unique_ptr<Page>> Pages;
};

class WikiNetwork
{
    public:
        WikiNetwork(){};
        WikiNetwork(std::vector<std::filesystem::path> &paths);
        void Populate(std::vector<std::filesystem::path> &paths);
    private:
        std::unordered_map<std::string,WikiNode> Nodes;
};