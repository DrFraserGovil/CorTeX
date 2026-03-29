#pragma once

#include <map>

template<class T, class U>
class BiAccessMap
{
    public:
        BiAccessMap()=default;
        BiAccessMap(std::map<T,U> input) : Forward(input)
        {
            ConstructReverse();
        }

        void Insert(const T & t, const U & u)
        {
            Forward[t] = u;
            Reverse[u] = t;
        }

        auto Erase(typename std::map<U, T>::iterator it)
        {
            Forward.erase(it->second); 
            return Reverse.erase(it);  
        }
        auto Erase(typename std::map<T, U>::iterator it)
        {
            Reverse.erase(it->second); 
            return Forward.erase(it);  
        }

        void Erase(const T & t)
        {
            auto it = Forward.find(t);
            if (it!=Forward.end())
            {
                Reverse.erase(it->second);
                Forward.erase(it);
            }   
        }
        void Erase(const U & u)
        {
            auto it = Reverse.find(u);
            if (it!=Reverse.end())
            {
                Forward.erase(it->second);
                Reverse.erase(it);
            }   
        }

        const U & operator[](T index) const
        {
            return Forward.at(index);
        }

        const T & operator[](U index) const
        {
            return Reverse.at(index);
        }

        auto begin()
        {
            return Forward.begin();
        }

        auto end()
        {
            return Forward.end();
        }

        auto rbegin()
        {
            return Reverse.begin();
        }
        auto rend()
        {
            return Reverse.end();
        }

        auto LowerBound(const T & index)
        {
            return Forward.lower_bound(index);
        }
        auto LowerBound(const U & index)
        {
            return Reverse.upper_bound(index);
        }

        bool Contains(const T & index)
        {
            return Forward.contains(index);
        }
        bool Contains(const U & index)
        {
            return Reverse.contains(index);
        }
        size_t size()
        {
            return Forward.size();
        }
    private:
        std::map<T,U> Forward;
        std::map<U,T> Reverse;
        void ConstructReverse()
        {
            Reverse.clear();
            for (auto & p: Forward)
            {
                Reverse.insert(p.second,p.first);
            }
        }
};