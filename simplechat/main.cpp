
#if !defined(_MSC_VER)
#include <introspection/not_win32.h>
#endif
#include <introspection/sample_chat.h>
#include <stdlib.h>

EXTERN_PROTOCOL(my_proto);

void usage()
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "samplechat server p              -- start serving on port p\n");
    fprintf(stderr, "samplechat edit                  -- edit user file\n");
    fprintf(stderr, "samplechat client user server p  -- connect to server, port p, as user user\n");
    fprintf(stderr, "The server uses a file named 'users.txt' for name/password information.\n");
    exit(1);
}

void do_client(int argc, char const *argv[]);
void do_server(int argc, char const *argv[]);
void do_edit(int argc, char const *argv[]);

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        usage();
    }
    if (!strcmp(argv[1], "client")) {
        do_client(argc-1, argv+1);
    }
    else if (!strcmp(argv[1], "server")) {
        do_server(argc-1, argv+1);
    }
    else if (!strcmp(argv[1], "edit")) {
        do_edit(argc-1, argv+1);
    }
    else {
        usage();
    }
    return 0;
}


