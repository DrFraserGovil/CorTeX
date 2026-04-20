#include "aliases.h"
#include <algorithm>
#include "../global.h"

int path_distancer(fs::path a, fs::path b)
{
    if (a == b) return  std::numeric_limits<int>::max(); //if the paths are identical, then we want to ignore this link as a potential target, since it would be a circular reference. Returning max int ensures that it will never be chosen as the closest link
    auto parent_a = a.parent_path().lexically_normal();
    auto parent_b = b.parent_path().lexically_normal();

    if (parent_a == parent_b)
    {
        return 0;
    }

    auto relative = parent_a.lexically_relative(parent_b);
    if (relative.empty())
    {
        return -1; //since we returned 0 when parent paths were equal, an empty relative path means they are on different root paths (e.g. different drives on Windows)
    }

    return std::distance(relative.begin(),relative.end());
}


AliasEntry::AliasEntry(std::string key, std::weak_ptr<Note> target) : Key(key)
{
    Targets.push_back(target);
}

bool AliasEntry::Contains(std::weak_ptr<Note> target)
{
    for (auto & existing : Targets)
    {
        if (existing.lock() == target.lock())
        {
            return true;
        }
    }
    return false;
}

bool AliasEntry::Remove(std::weak_ptr<Note> target)
{
    for (auto it = Targets.begin(); it != Targets.end(); ++it)
    {
        if (it->lock() == target.lock())
        {
            Targets.erase(it);
            return Targets.size()==0;
        }
    }
    return false;
}

void AliasEntry::Add(std::weak_ptr<Note> target)
{
    Targets.push_back(target);
}

std::weak_ptr<Note> AliasEntry::GetClosestLink(fs::path requestingFile)
{
    if (Targets.size() == 1)
    {
        // LOG(DEBUG) << "\tMatched " << Key << " to " << Targets[0].lock()->Header.Title << " for " << requestingFile.string();
        return Targets[0];
    }

    std::weak_ptr<Note> closest;
    int closestDistance = std::numeric_limits<int>::max();

    for (auto & target : Targets)
    {
        if (auto targetPtr = target.lock()) //check weak_ptr still exists (deliberate '='!)
        {
            int distance = path_distancer(requestingFile,targetPtr->Path.Compile);
            if (distance != -1 && distance <= closestDistance)
            {
                closestDistance = distance;
                closest = target;
            }
        }
    }
    LOG(DEBUG) << Cortex.Colours.DebugRed << "\tAlias clash for '" << Key << Cortex.Colours.DebugDefault << "'\n\t\tLink Origin: " << requestingFile.string() << "\n\t\tResolved to: " << closest.lock()->Path.Compile.string();

    return closest;
}



void AliasList::Sync(std::weak_ptr<Note> target)
{
    std::set<std::string> existingKeys;
    for (auto &[key, entry] : Aliases)
    {
        if (entry.Contains(target))
        {
            existingKeys.insert(key);
        }
    }
    std::set<std::string> newKeys(target.lock()->Header.Aliases.begin(),target.lock()->Header.Aliases.end());

    std::set<std::string> keysNotInIndex;
    std::set_difference(newKeys.begin(),newKeys.end(),existingKeys.begin(),existingKeys.end(),std::inserter(keysNotInIndex,keysNotInIndex.end()));

    for (auto & alias: keysNotInIndex)
    {
        if (Aliases.contains(alias))
        {
            Aliases[alias].Add(target);
        }
        else
        {
            Aliases[alias] = AliasEntry(alias,target);
        }
    }

    std::set<std::string> deletedKeys;
    std::set_difference(existingKeys.begin(),existingKeys.end(),newKeys.begin(),newKeys.end(),std::inserter(deletedKeys,deletedKeys.end()));
    
    for (auto & alias: deletedKeys)
    {
        LOG(DEBUG) << "Removing alias " << alias << " for " << target.lock()->Path.Source.string();
        if (Aliases.contains(alias))
        {
            Aliases[alias].Remove(target);
            if (Aliases[alias].Targets.size() == 0)
            {
                Aliases.erase(alias);
            }
        }
    }
    //     if (!Aliases.contains(alias))
    //     {
    //         Aliases[alias] = AliasEntry(alias,target);
    //     }
    //     else
    //     {
    //         Aliases[alias].Add(target);
    //     }
    // }
    // LOG(DEBUG) << "Sync complete";
}

void AliasList::Remove(std::weak_ptr<Note> target)
{
    std::set<std::string> removable;
    for (auto alias : Aliases)
    {
        if (alias.second.Remove(target))
        {
            removable.insert(alias.first); //need to delay removal so that the iteration doesn't get broken
        }
    }

    for (auto & remove : removable)
    {
        Aliases.erase(remove);
    }
}

std::weak_ptr<Note> AliasList::GetLink(std::string_view & key, fs::path requestingFile)
{
    const std::string keyStr = (std::string)key;
    if (Aliases.contains(keyStr))
    {
        return Aliases[keyStr].GetClosestLink(requestingFile);
    }
    else
    {
        return std::weak_ptr<Note>{};
    }
}