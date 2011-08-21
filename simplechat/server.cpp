
#if !defined(_MSC_VER)
#include <introspection/not_win32.h>
#else
#include <WinSock2.h>
#include <Windows.h>
typedef int w32_socklen_t;
#endif
#include <assert.h>
#include <time.h>
#include <introspection/sample_chat.h>
#include <map>
#include "userlist.h"
#include "refptr.h"

#define TRACE(x) printf("%s:%d: %s\n", __FILE__, __LINE__, #x)

EXTERN_PROTOCOL(my_proto);

/* 15 minutes with no activity means you get the boot */
const long TIMEOUT_CONNECTED = 60*15;
/* Half a minute without authentication means you get the boot */
const long TIMEOUT_NONCONNECTED = 30;

/* this is somewhat arbitrary, although the size of the login message and 
   the 64-socket Windows select() limit does play into this particular socket 
   implementation.
   */
const int MAX_USER_COUNT = 32;

/* Keep track of users connected, or attempting to connect, to the service
  */
class ConnectedUser
{
    public:
        ConnectedUser(int sockfd) :
            sockfd_(sockfd),
            gotinfo_(false),
            isdead_(false),
            qoff_(0),
            qsize_(0),
            ooff_(0),
            osize_(0)
        {
            time(&lastTime_);
            dispatcher_.add_handler(my_proto, this, &ConnectedUser::OnLogin);
            dispatcher_.add_handler(my_proto, this, &ConnectedUser::OnSaySomething);
        }
        ~ConnectedUser();

        void service();
        void drain();
        void decode_one(void const *buf, size_t size);
        void kick(char const *reason);
        void enqueue(void const *buf, size_t size);
        bool is_dead()
        {
            return isdead_;
        }
        bool is_timed_out(time_t now)
        {
            return now > lastTime_ + (gotinfo_ ? TIMEOUT_CONNECTED : TIMEOUT_NONCONNECTED);
        }

        int sockfd_;
        bool gotinfo_;
        bool isdead_;
        UserInfo info_;
        unsigned char buf_[4096];
        int qoff_;
        int qsize_;
        time_t lastTime_;
        unsigned char obuf_[8192];
        int ooff_;
        int osize_;

        introspection::dispatch_t dispatcher_;

        void OnLogin(LoginPacket const &lp);
        void OnSaySomething(SaySomethingPacket const &ssp);
};


class QueuedPacket
{
    public:
        virtual ~QueuedPacket() {}
        virtual void emit(introspection::stream &ss) = 0;
};

static std::map<int, ref_ptr<ConnectedUser> > users;
static int port;
static int asock;
static std::list<ref_ptr<QueuedPacket> > queue;

template<typename T>
class QueuedPacketImpl : public QueuedPacket
{
    public:
        QueuedPacketImpl(T const &t) :
            t_(t)
        {
        }
        T t_;
        void emit(introspection::stream &ss)
        {
            my_proto.encode(t_, ss);
        }
};

template<typename T>
void enqueue_outgoing(T const &t)
{
    queue.push_back(ref_ptr<QueuedPacket>(new QueuedPacketImpl<T>(t)));
}

ConnectedUser::~ConnectedUser()
{
    closesocket(sockfd_);
    if (gotinfo_)
    {
        UserLeftPacket ulp;
        ulp.who = info_.name;
        enqueue_outgoing(ulp);
    }
}

void ConnectedUser::service()
{
    int r = recv(sockfd_, (char *)&buf_[qoff_ + qsize_], sizeof(buf_)-qoff_-qsize_, 0);
    if (r < 1)
    {
        //  this could be a legit disconnect
        fprintf(stderr, "lost connection to %s: %d\n", info_.name.c_str(), WSAGetLastError());
        isdead_ = true;
    }
    else
    {
        qsize_ += r;
maybe_more:
        if (qsize_ >= 2)
        {
            int len = (buf_[qoff_] << 8) | buf_[qoff_ + 1];
            if (qsize_ >= len + 2)
            {
                decode_one(&buf_[qoff_ + 2], len);
                qoff_ += 2 + len;
                qsize_ -= 2 + len;
                goto maybe_more;
            }
            else if (qoff_ > 0)
            {
                memmove(buf_, &buf_[qoff_], qsize_);
                qoff_ = 0;
            }
            else
            {
                if (len > sizeof(buf_) - 2)
                {
                    //  this means he's sending junk packets
                    kick("bad frame size");
                }
            }
        }
    }
}

void ConnectedUser::decode_one(void const *buf, size_t size)
{
    try
    {
        introspection::readonly_stream rs(buf, size);
        //  size of max packet struct in RAM
        char pack[256];
        int d = my_proto.decode(pack, sizeof(pack), rs);
        dispatcher_.dispatch(d, pack);
    }
    catch (std::exception const &x)
    {
        kick(x.what());
    }
}

void ConnectedUser::enqueue(void const *data, size_t size)
{
    if (size + osize_ > sizeof(obuf_))
    {
        //  this means his networking is lagged out or disconnected
        kick("failed to drain send buffer in a timely fashion");
    }
    else
    {
        if (size + osize_ + ooff_ > sizeof(obuf_))
        {
            memmove(obuf_, &obuf_[ooff_], osize_);
            ooff_ = 0;
        }
        memcpy(&obuf_[ooff_ + osize_], data, size);
        osize_ += size;
    }
}

void ConnectedUser::drain()
{
    int w = ::send(sockfd_, (char const *)&obuf_[ooff_], osize_, 0);
    if (w < 1)
    {
        //  this means his networking is lagged out or disconnected
        kick("failed to send on socket");
    }
    else
    {
        ooff_ += w;
        osize_ -= w;
    }
}

void ConnectedUser::kick(char const *reason)
{
    fprintf(stderr, "%s: kicking %s\n", reason, info_.name.c_str());
    isdead_ = true;
    qsize_ = 0;
    qoff_ = 0;
}

void ConnectedUser::OnLogin(LoginPacket const &lp)
{
    UserInfo ui;
    /* Verify that the user exists. I don't use a password.
       */
    if (!get_user_by_name(lp.name.c_str(), ui))
    {
        kick("unknown user name");
        return;
    }
    ConnectedPacket cp;
    cp.result = 1;
    cp.version = 1;
    for (std::map<int, ref_ptr<ConnectedUser> >::iterator ptr(users.begin()), end(users.end());
        ptr != end; ++ptr)
    {
        if ((*ptr).second->info_.name == ui.name)
        {
            kick("already connected");
            return;
        }
        if ((*ptr).second->gotinfo_)
        {
            cp.users.push_back((*ptr).second->info_.name);
        }
    }
    gotinfo_ = true;
    info_ = ui;
    //  send the response to the user
    simple_stream ss;
    ss.write_bytes(2, "\0");    //  space for frame size
    my_proto.encode(cp, ss);
    unsigned char *p = (unsigned char *)ss.unsafe_data();
    size_t sz = ss.position() - 2;
    p[0] = (sz >> 8) & 0xff;
    p[1] = sz & 0xff;
    enqueue(p, sz + 2);

    ss.set_position(0);
    ss.truncate_at_pos();
    UserJoinedPacket ujp;
    ujp.who = info_.name;
    enqueue_outgoing(ujp);
}

void ConnectedUser::OnSaySomething(SaySomethingPacket const &ssp)
{
    if (!gotinfo_)
    {
        kick("attempt to speak before being logged in");
        return;
    }
    SomeoneSaidSomethingPacket sssp;
    sssp.who = info_.name;
    sssp.what = ssp.message;
    if (sssp.what.size() > 100)
    {
        //  truncate to the maximum allowed size per message (avoid over-spamming)
        sssp.what.resize(97);
        sssp.what += "...";
    }
    enqueue_outgoing(sssp);
}


void accept_one()
{
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    w32_socklen_t len = sizeof(sin);
    int sock = accept(asock, (sockaddr *)&sin, &len);
    if (sock < 0)
    {
        fprintf(stderr, "error accepting socket: %d\n", WSAGetLastError());
        return;
    }
    if (users.size() >= MAX_USER_COUNT)
    {
        //  just disconnect -- not enough capacity
        closesocket(sock);
        return;
    }
    users[sock] = ref_ptr<ConnectedUser>(new ConnectedUser(sock));
}

void service_loop()
{
    simple_stream ss;
    ss.write_bytes(2, "\0");
    //  limit the max size of an individual frame
    while (queue.size() > 0 && ss.position() < 2000)
    {
        TRACE(emit);
        queue.front()->emit(ss);
        queue.pop_front();
    }
    size_t sz = ss.position() - 2;
    unsigned char *p = (unsigned char *)ss.unsafe_data();
    p[0] = (sz >> 8) & 0xff;
    p[1] = sz & 0xff;

    fd_set fdrd, fdwr;
    FD_ZERO(&fdrd);
    FD_ZERO(&fdwr);
    FD_SET(asock, &fdrd);
    int top = asock;
    for (std::map<int, ref_ptr<ConnectedUser> >::iterator ptr(users.begin()), end(users.end());
            ptr != end; ++ptr)
    {
        if (sz > 0)
        {
            TRACE(enqueue);
            (*ptr).second->enqueue(ss.unsafe_data(), ss.position());
        }
        FD_SET((*ptr).first, &fdrd);
        if ((*ptr).second->osize_ > 0)
        {
            FD_SET((*ptr).first, &fdwr);
        }
        if ((*ptr).first > top)
        {
            top = (*ptr).first;
        }
    }
    TRACE(select);
    select(top+1, &fdrd, &fdwr, 0, 0);
    TRACE(select_done);
    if (FD_ISSET(asock, &fdrd))
    {
        TRACE(accept_one);
        accept_one();
    }
    std::list<int> dieing;
    time_t now;
    time(&now);
    for (std::map<int, ref_ptr<ConnectedUser> >::iterator ptr(users.begin()), end(users.end());
        ptr != end; ++ptr)
    {
        if (FD_ISSET((*ptr).first, &fdwr))
        {
            TRACE(drain);
            (*ptr).second->drain();
        }
        if (FD_ISSET((*ptr).first, &fdrd))
        {
            TRACE(service);
            (*ptr).second->service();
        }
        if ((*ptr).second->is_dead() || (*ptr).second->is_timed_out(now))
        {
            TRACE(dieing);
            dieing.push_back((*ptr).first);
        }
    }
    for (std::list<int>::iterator ptr(dieing.begin()), end(dieing.end());
        ptr != end; ++ptr)
    {
        TRACE(erase);
        users.erase(*ptr);
    }
}

static void usage()
{
    fprintf(stderr, "usage: server port\n");
}

void do_server(int argc, char const *argv[])
{
    if (!load_userlist())
    {
        fprintf(stderr, "can't load userlist.txt -- please create one with 'edit' first\n");
        return;
    }
    if (argc != 2)
    {
        usage();
        return;
    }
    port = atoi(argv[1]);
    if (port < 1 || port > 65535)
    {
        usage();
        return;
    }
    
    WSADATA wsad;
    memset(&wsad, 0, sizeof(wsad));
    if (WSAStartup(MAKEWORD(2, 2), &wsad) < 0)
    {
        fprintf(stderr, "Could not start WinSock\n");
        return;
    }
    asock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (asock < 0)
    {
        fprintf(stderr, "Could not create the accepting socket: %d\n", WSAGetLastError());
        return;
    }
    /* don't wait for the two minute timeout before allowing re-bind after a stopped server is re-started */
    BOOL one = 1;
    setsockopt(asock, SOL_SOCKET, SO_REUSEADDR, (char const *)&one, sizeof(one));
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (bind(asock, (sockaddr const *)&sin, sizeof(sin)) < 0)
    {
        fprintf(stderr, "Could not bind the accepting socket to port %d: %d\n", port, WSAGetLastError());
        return;
    }
    if (listen(asock, 10) < 0)
    {
        fprintf(stderr, "listen() failed -- this almost never happens: %d\n", WSAGetLastError());
        return;
    }

    while (true)
    {
        service_loop();
    }
}
