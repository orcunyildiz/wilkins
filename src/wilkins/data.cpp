#include <wilkins/data.hpp>

static wilkins::NameMap* namemap = 0;

void
wilkins::
wilkins_set_namemap(void* nm)
{
    namemap = (wilkins::NameMap*) nm;
}

void
wilkins::save(const char* name, Value x)
{
    if (!namemap) return;
    wilkins::Value v = x;
    namemap->add(name, x);
}

bool
wilkins::exists(const std::string& name)
{
    if (!namemap) return false;
    return namemap->exists(name);
}

wilkins::NameMap*
wilkins::get_namemap()
{
    return namemap;
}

wilkins::Value
wilkins::load(const std::string& name)
{
    if (!get_namemap()) return Value();
    return get_namemap()->get(name);
}

int
wilkins::load_int(const std::string& name)
{
    if (!get_namemap()) return 0;

    int ret = wilkins::get<int>(namemap->get(name));

    if (ret < 0 || ret > 12) return 0;
    return ret;
}

void
wilkins::load_ptr(const char* name, void** ptr)
{
    if (!namemap) return;
    *ptr = wilkins::get<void*>(namemap->get(name));
}
