
#include <introspection/sample_chat.h>
#include <stdio.h>
#include <vector>

static char const *userlist_path;
static std::vector<UserInfo> userlist;

bool load_userlist()
{
    char const *names[] = {
        "userlist.txt",
        "../userlist.txt",
        "../../userlist.txt",
        "../../../userlist.txt"
    };
    FILE *f = 0;
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        f = fopen(names[i], "rb");
        if (f != NULL)
        {
            userlist_path = names[i];
            break;
        }
    }
    if (f == NULL)
    {
        return false;
    }
    userlist.clear();
    while (true)
    {
        char line[1024];
        line[0] = 0;
        fgets(line, 1024, f);
        if (line[0] == 0 || line[0] == '\n')
        {
            break;
        }
        UserInfo ui;
        ui.member_info().access().from_text(&ui, line);
        userlist.push_back(ui);
    }
    fclose(f);
    return true;
}

bool save_userlist()
{
    if (userlist_path == NULL)
    {
        userlist_path = "userlist.txt";
    }
    FILE *f = fopen(userlist_path, "wb");
    if (f == NULL)
    {
        return false;
    }
    for (std::vector<UserInfo>::iterator ptr(userlist.begin()), end(userlist.end());
            ptr != end; ++ptr)
    {
        std::string txt;
        (*ptr).member_info().access().to_text(&*ptr, txt);
        fprintf(f, "%s\n", txt.c_str());
    }
    fclose(f);
    return true;
}

unsigned int count_users()
{
    return userlist.size();
}

void get_user_by_index(unsigned int index, UserInfo &ui)
{
    if (index >= userlist.size())
    {
        throw std::runtime_error("get_user_by_index(): index out of range");
    }
    ui = userlist[index];
}

bool get_user_by_name(char const *name, UserInfo &ui)
{
    for (std::vector<UserInfo>::iterator ptr(userlist.begin()), end(userlist.end());
            ptr != end; ++ptr)
    {
        if ((*ptr).name == name)
        {
            ui = *ptr;
            return true;
        }
    }
    return false;
}

bool update_user_by_index(unsigned int index, UserInfo const &ui)
{
    if (index >= userlist.size())
    {
        return false;
    }
    for (std::vector<UserInfo>::iterator ptr(userlist.begin()), end(userlist.end());
            ptr != end; ++ptr)
    {
        if ((*ptr).name == ui.name && ptr-userlist.begin() != index)
        {
            return false;
        }
    }
    userlist[index] = ui;
    return true;
}

void delete_user_by_index(unsigned int index)
{
    if (index >= userlist.size())
    {
        throw std::runtime_error("get_user_by_index(): index out of range");
    }
    userlist.erase(userlist.begin() + index);
}

bool new_user(UserInfo const &ui)
{
    for (std::vector<UserInfo>::iterator ptr(userlist.begin()), end(userlist.end());
            ptr != end; ++ptr)
    {
        if ((*ptr).name == ui.name)
        {
            return false;
        }
    }
    userlist.push_back(ui);
    return true;
}

