
#if defined(_MSC_VER)
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <introspection/not_win32.h>
#endif
#include <stdio.h>
#include <assert.h>
#include <introspection/sample_chat.h>


EXTERN_PROTOCOL(my_proto);

/* some working global variables */
static std::string username;
static std::string servername;
static int port;
static int sockfd = -1;
static bool connected = false;
static volatile bool running = true;


/* the dispatcher delegates to an object and member function I designate 
   for each kind of packet that comes in */
static introspection::dispatch_t dispatcher;

/* define a function for each packet type that I want to see coming in */
class ClientHandler
{
    public:
        void OnConnected(ConnectedPacket const &cp)
            {
                fprintf(stderr, "User List:\n");
                for (std::list<std::string>::const_iterator ptr(cp.users.begin()), end(cp.users.end());
                    ptr != end; ++ptr)
                {
                    fprintf(stderr, "%s\n", (*ptr).c_str());
                }
            }
        void OnSomeoneSaidSomething(SomeoneSaidSomethingPacket const &sssp)
            {
                //  don't show my own messages
                if (sssp.who != username)
                {
                    fprintf(stderr, "%s: %s\n", sssp.who.c_str(), sssp.what.c_str());
                }
            }
        void OnUserJoined(UserJoinedPacket const &ujp)
            {
                fprintf(stderr, "%s joined\n", ujp.who.c_str());
            }
        void OnUserLeft(UserLeftPacket const &ulp)
            {
                fprintf(stderr, "%s left\n", ulp.who.c_str());
            }
};

static ClientHandler handler;

static void setup_dispatch()
{
    /* add the four packets I'm prepared to deal with coming in to the dispatcher */
    dispatcher.add_handler<ConnectedPacket>(my_proto, &handler, &ClientHandler::OnConnected);
    dispatcher.add_handler<SomeoneSaidSomethingPacket>(my_proto, &handler, &ClientHandler::OnSomeoneSaidSomething);
    dispatcher.add_handler<UserJoinedPacket>(my_proto, &handler, &ClientHandler::OnUserJoined);
    dispatcher.add_handler<UserLeftPacket>(my_proto, &handler, &ClientHandler::OnUserLeft);
}

static void decode_and_handle(void *data, size_t size)
{
    introspection::readonly_stream ss(data, size);
    char buf[256]; //  size of biggest struct in memory
    while (ss.bytes_left() > 0)
    {
        /* decode the marshaled data based on the type code; return the type code */
        int d = my_proto.decode(buf, sizeof(buf), ss);
        /* dispatch to a handler based on the type code */
        dispatcher.dispatch(d, buf);
    }
}

static void send_login()
{
    LoginPacket lp;
    lp.name = username;
    lp.password = "";
    lp.version = 1;
    simple_stream ss;
    ss.write_bytes(2, "\0");
    my_proto.encode(lp, ss);
    unsigned char *p = (unsigned char *)ss.unsafe_data();
    size_t sz = ss.position() - 2;
    p[0] = (sz >> 8) & 0xff;
    p[1] = sz & 0xff;
    if (send(sockfd, (char const *)p, sz + 2, 0) != (sz + 2))
    {
        fprintf(stderr, "error sending login packet: %d\n", WSAGetLastError());
    }
}

static void thread_func(void *)
{
    /* look up the server */
    struct hostent *hent = gethostbyname(servername.c_str());
    if (!hent)
    {
        fprintf(stderr, "%s: host not found\n", servername.c_str());
        return;
    }
    sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    memcpy(&sad.sin_addr, hent->h_addr_list[0], 4);
    sad.sin_port = htons(port);

    /* connect to the server using TCP */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        fprintf(stderr, "socket creation failure\n");
        return;
    }
    if (connect(sockfd, (sockaddr *)&sad, sizeof(sad)) < 0)
    {
        fprintf(stderr, "connect() failed. Is the host and port right?\n");
        return;
    }
    connected = true;
    fprintf(stderr, "connected; logging in...\n");

    send_login();

    //  receive packets, decode, and print, while running
    int qoff = 0;   //  offset to first unprocessed byt
    int qsize = 0;  //  number of unprocessed bytes
    unsigned char buf[4096];
    while (running)
    {
        /* The data on the wire is marshaled as one big-endian short for byte 
           count, and then that many bytes of payload. Repeat. Each payload 
           is one or more packets within the my_proto protocol.
           */
        int r = recv(sockfd, (char *)&buf[qoff + qsize], sizeof(buf)-qoff-qsize, 0);
        if (r == 0)
        {
            //  remote close
            break;
        }
        if (r < 0)
        {
            /* WSAGetLastError() may have been cleared by the other thread... */
            fprintf(stderr, "receive error: %d\n", WSAGetLastError());
            break;
        }
        qsize += r;
        bool processed_one = false;
    maybe_more:
        if (qsize >= 2)
        {
            int len = (buf[qoff] << 8) | buf[qoff+1];
            if (len > sizeof(buf) - 2)
            {
                /*  No packet should be that big, according to my arbitrary protocol.
                    Note that this limits the number of users in the connected packet 
                    user list!
                    */
                fprintf(stderr, "received packet size %d: protocol error\n", len);
                break;
            }
            else if (qsize >= 2 + len)
            {
                processed_one = true;
                //  decode, because I have at least one full frame
                decode_and_handle(&buf[qoff + 2], len);
                qsize -= 2 + len;
                qoff += 2 + len;
                goto maybe_more;
            }
            else if (qoff > 0)
            {
                //  shift over to make more space
                memmove(buf, &buf[qoff], qsize);
                qoff = 0;
            }
            else
            {
                assert(processed_one);
            }
        }
    }
    fprintf(stderr, "connection closed\n");
    closesocket(sockfd);
    running = false;
}

static bool send_a_message(char const *line)
{
    SaySomethingPacket ssp;
    ssp.message = line;
    simple_stream ss;
    //  make space for the frame size field (short)
    ss.write_bytes(2, "\0");
    my_proto.encode(ssp, ss);
    size_t plen = ss.position() - 2;
    unsigned char *pdata = (unsigned char *)ss.unsafe_data();
    //  generate a big-endian short for number of bytes count
    pdata[0] = (plen >> 8) & 0xff;
    pdata[1] = plen & 0xff;
    int l = send(sockfd, (char const *)pdata, plen + 2, 0);
    if (l < 0)
    {
        /* WSAGetLastError() may have been cleared by the other thread... */
        fprintf(stderr, "send error: %d\n", WSAGetLastError());
    }
    return l == plen + 2;
}

static void usage()
{
    fprintf(stderr, "usage: client username servername port\n");
}

void do_client(int argc, char const *argv[])
{
    /* parse arguments */
    if (argc != 4)
    {
        usage();
        return;
    }
    username = argv[1];
    servername = argv[2];
    port = atoi(argv[3]);
    if (port < 1 || port > 65535)
    {
        usage();
        return;
    }

    /* initialize winsock */
    WSADATA wsad;
    memset(&wsad, 0, sizeof(wsad));
    if (WSAStartup(MAKEWORD(2, 2), &wsad) != 0)
    {
        fprintf(stderr, "WSAStartup() failed\n");
        return;
    }

    /* initialize the client handler */
    setup_dispatch();
    fprintf(stderr, "Waiting for connection. Use 'quit' to quit.\n");
    uintptr_t thread = _beginthread(&thread_func, 65536, 0);

    /* run the read-send loop */
    while (running)
    {
        char line[1024];
        fprintf(stderr, "\nsay> ");
        fgets(line, 1024, stdin);
        line[1023] = 0;
        size_t len = strlen(line);
        if (len == 0)
        {
            continue;
        }
        if (line[len-1] == '\n')
        {
            line[len-1] = 0;
            --len;
            if (len == 0)
            {
                continue;
            }
        }
        if (!strcmp(line, "quit"))
        {
            //  force a close
            break;
        }
        //  create and send a "say" packet
        if (!connected)
        {
            fprintf(stderr, "Not connected -- packet not sent. Use 'quit' to quit.\n");
            continue;
        }
        if (!send_a_message(line))
        {
            /* With this data rate, we won't be able to jam the TCP send buffer 
               full, so any problems with enqueuing all the data means something's 
               gone wrong, and our best bet is to disconnect.
               */
            fprintf(stderr, "send error: connection trouble?\n");
            break;
        }
    }
    /* wait for the thread to close */
    bool wasrunning = running;
    running = false;
    closesocket(sockfd);
    if (wasrunning)
    {
        fprintf(stderr, "waiting for thread...\n");
    }
    ::WaitForSingleObject((HANDLE)thread, 5000);
    /* if somehow the thread didn't close, return anyway... */
}
