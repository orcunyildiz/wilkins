#ifndef WILKINS_DATA_HPP
#define WILKINS_DATA_HPP

#include <string>
#include <map>
#include <queue>
#include <vector>
#include <memory>

#include "variant.hpp"

#ifdef __cplusplus
extern "C" {
#endif

namespace wilkins
{

using mpark::variant;
using mpark::get;

using Value = variant<int,void*>;

class NameMap
{
    public:
        typedef     std::map<std::string, Value>                          DataMap;
        typedef     std::queue<Value>                                     ValueQueue;
        typedef     std::map<std::string, ValueQueue>                     QueuesMap;

    public:
        void        add(const std::string& name, Value value)
        {
            auto it = queues_.find(name);
            if (it != queues_.end())
                it->second.push(value);
            else
                values_[name] = value;
        }

        Value       get(const std::string& name)
        {
            auto it = queues_.find(name);
            if (it != queues_.end())
            {
                auto& q = it->second;
                Value v = q.front();
                q.pop();
                return v;
            }
            else return
                values_.find(name)->second;
        }

        void        create_queue(const std::string& name)               { queues_.emplace(name, ValueQueue()); }
        bool        queue_empty(const std::string& name) const          { return queues_.find(name)->second.empty(); }

        bool        exists(const std::string& name) const               { return values_.find(name) != values_.end() || queues_.find(name) != queues_.end(); }

        void        clear()                                             { values_.clear(); queues_.clear(); }

    private:
        DataMap     values_;
        QueuesMap   queues_;
};

bool        exists(const std::string& name);

void        save(const char* name, Value x);

Value       load(const std::string& name);

int         load_int(const std::string& name);

void        load_ptr(const char* name, void** ptr);

void        wilkins_set_namemap(void* nm);

NameMap*    get_namemap();

}

#ifdef __cplusplus
}
#endif

#endif
