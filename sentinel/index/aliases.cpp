#include "aliases.h"
#include "../constants.h"


int path_distancer(fs::path a, fs::path b)
{
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
    // for (auto & existing : Targets)
    // {
    //     if (existing.lock() == target.lock())
    //     {
    //         return;
    //     }
    // }
    Targets.push_back(target);
}

std::weak_ptr<Note> AliasEntry::GetClosestLink(fs::path requestingFile)
{
    std::weak_ptr<Note> closest;
    int closestDistance = std::numeric_limits<int>::max();

    for (auto & target : Targets)
    {
        if (auto targetPtr = target.lock()) //check weak_ptr still exists (deliberate '='!)
        {
            int distance = path_distancer(requestingFile,targetPtr->Path.Source);
            if (distance != -1 && distance < closestDistance)
            {
                closestDistance = distance;
                closest = target;
            }
        }
    }

    return closest;
}



void AliasList::Sync(std::weak_ptr<Note> target)
{
    LOG(DEBUG) << "Beginning sync for " << target.lock()->Path.Source.string();
    Remove(target); //remove the old links for this note (if any)
    LOG(DEBUG) << "Removal complete";
    for (auto & alias: target.lock()->Header.Aliases)
    {
        if (!Aliases.contains(alias))
        {
            Aliases[alias] = AliasEntry(alias,target);
        }
        else
        {
            Aliases[alias].Add(target);
        }
    }
    LOG(DEBUG) << "Sync complete";
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