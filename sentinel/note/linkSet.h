#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <deque>
#include "link.h"

class Note; //forward declaration

struct CaseInsensitiveLess {
    using is_transparent = void; // Enable string_view lookups

    bool operator()(std::string_view lhs, std::string_view rhs) const {
        return std::lexicographical_compare(
            lhs.begin(), lhs.end(),
            rhs.begin(), rhs.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) < 
                       std::tolower(static_cast<unsigned char>(b));
            }
        );
    }
};

class LinkSet
{
    public:
        std::set<std::string> Orphaned;
        std::map<std::string, std::weak_ptr<Note>,CaseInsensitiveLess> Outbound;
        std::vector<Link> Parsed;
        std::deque<int> Lines;

        LinkSet();
        void Parse(std::vector<std::string> & content);
        void Reset();
};