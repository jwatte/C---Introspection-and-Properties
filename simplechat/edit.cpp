
#pragma warning(disable: 4996)
#pragma warning(disable: 4018)

#if !defined(_MSC_VER)
#include <introspection/not_win32.h>
#endif
#include <introspection/sample_chat.h>
#include <assert.h>
#include "userlist.h"

static void help()
{
    fprintf(stderr, "editor help:\n");
    fprintf(stderr, "list      -- print all users\n");
    fprintf(stderr, "del #     -- delete a user\n");
    fprintf(stderr, "edit #    -- edit a user\n");
    fprintf(stderr, "new       -- new user\n");
    fprintf(stderr, "save      -- save and quit\n");
    fprintf(stderr, "abort     -- quit without saving\n");
    fprintf(stderr, "help      -- this help text\n");
}

static bool running = true;

void do_list(char const *str)
{
    UserInfo ui;
    fprintf(stdout, "## ");
    for (introspection::member_t const *ptr = ui.member_info().begin(), *end = ui.member_info().end(); 
        ptr != end; ++ptr)
    {
        fprintf(stdout, "%-20s", ptr->name());
    }
    fprintf(stdout, "\n");
    for (size_t i = 0, n = count_users(); i != n; ++i)
    {
        fprintf(stdout, "%-3d", i);
        get_user_by_index(i, ui);
        for (introspection::member_t const *ptr = ui.member_info().begin(), *end = ui.member_info().end(); 
            ptr != end; ++ptr)
        {
            std::string ostr;
            //  offset is already baked into the accessor
            ptr->access().to_text(&ui, ostr);
            fprintf(stdout, "%-20s", ostr.c_str());
        }
        fprintf(stdout, "\n");
    }
}

void do_del(char const *str)
{
    int ix = -1;
    if (1 != sscanf(str, " %d", &ix))
    {
        fprintf(stderr, "usage: del <index>\n");
        return;
    }
    if (ix < 0 || ix >= count_users())
    {
        fprintf(stderr, "index %d is out of range [0, %d)\n", ix, count_users());
        return;
    }
    delete_user_by_index(ix);
}

void do_edit(char const *str)
{
    int ix = -1;
    if (1 != sscanf(str, " %d", &ix))
    {
        fprintf(stderr, "usage: edit <index>\n");
        return;
    }
    if (ix < 0 || ix >= count_users())
    {
        fprintf(stderr, "index %d is out of range [0, %d)\n", ix, count_users());
        return;
    }
    bool changed = false;
    UserInfo ui;
    get_user_by_index(ix, ui);
    for (introspection::member_t const *ptr = ui.member_info().begin(), *end = ui.member_info().end(); 
        ptr != end; ++ptr)
    {
    again:
        std::string totx;
        ptr->access().to_text(&ui, totx);
        fprintf(stderr, "%s [%s]: ", ptr->name(), totx.c_str());
        char buf[256];
        buf[0] = 0;
        fgets(buf, 256, stdin);
        size_t sl = strlen(buf);
        if (sl == 0 || buf[0] == '\n')
        {
            continue;
        }
        if (buf[sl-1] == '\n')
        {
            buf[sl-1] = 0;
        }
        try
        {
            ptr->access().from_text(&ui, buf);
            changed = true;
        }
        catch (std::exception &x)
        {
            fprintf(stderr, "format error: %s\n", x.what());
            fprintf(stderr, "use blank line for no change\n");
            goto again;
        }
    }
    if (!update_user_by_index(ix, ui))
    {
        fprintf(stderr, "Another user of that name already exists\n");
    }
}

void do_new(char const *str)
{
    UserInfo ui;
    for (introspection::member_t const *ptr = ui.member_info().begin(), *end = ui.member_info().end(); 
        ptr != end; ++ptr)
    {
    again:
        fprintf(stderr, "%s: ", ptr->name());
        char buf[256];
        buf[0] = 0;
        fgets(buf, 256, stdin);
        size_t sl = strlen(buf);
        if (sl == 0 || buf[0] == '\n')
        {
            fprintf(stderr, "interrupted\n");
            return;
        }
        if (buf[sl-1] == '\n')
        {
            buf[sl-1] = 0;
        }
        try
        {
            //  note: offset is already baked into the "access()" accessor
            ptr->access().from_text(&ui, buf);
        }
        catch (std::exception &x)
        {
            fprintf(stderr, "format error: %s\n", x.what());
            fprintf(stderr, "use blank line to interrupt\n");
            goto again;
        }
    }
    if (!new_user(ui))
    {
        fprintf(stderr, "A user of that name already exists\n");
    }
}

void do_save(char const *str)
{
    if (!save_userlist())
    {
        fprintf(stderr, "ERROR: could not save userlist.txt\n");
        return;
    }
    fprintf(stderr, "saved userlist.txt\n");
    running = false;
}

void do_abort(char const *str)
{
    running = false;
}

void do_help(char const *str)
{
    help();
}

static struct
{
    char const *name;
    void (*func)(char const *);
}
commands[] = {
    { "list", do_list },
    { "del", do_del },
    { "edit", do_edit },
    { "new", do_new },
    { "save", do_save },
    { "abort", do_abort },
    { "help", do_help },
};

void do_edit(int argc, char const *argv[])
{
    help();

    if (!load_userlist())
    {
        fprintf(stderr, "Error: could not open userlist.txt\n");
    }

    char line[1024];
    while (running)
    {
        fprintf(stderr, "edit> ");
        line[0] = 0;
        fgets(line, 1024, stdin);
        char cmd[256];
        if (1 != sscanf(line, "%s", cmd))
        {
            continue;
        }
        bool done = false;
        for (size_t i = 0, n = sizeof(commands)/sizeof(commands[0]); i != n; ++i)
        {
            if (!strcmp(cmd, commands[i].name))
            {
                (*commands[i].func)(line + strlen(cmd));
                done = true;
                break;
            }
        }
        if (!done)
        {
            fprintf(stderr, "use 'help' for help\n");
        }
    }
}
