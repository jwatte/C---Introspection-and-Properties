
#if !defined(introspection_not_win32_h)
#define introspection_not_win32_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef socklen_t w32_socklen_t;
typedef int BOOL;
typedef void *HANDLE;
struct WSADATA {
    int _unused;
};

inline unsigned short MAKEWORD(unsigned char a, unsigned char b) { return ((unsigned short)a << 8) | b; }
inline int WSAStartup(unsigned short us, WSADATA *wsad) { return 0; }
inline int WSAGetLastError() { return errno; }
inline int closesocket(int sock) { return close(sock); }

#include <pthread.h>

struct not_thread {
    char const *type;
    pthread_t thr;
    void *arg;
    void (*func)(void *);
};

inline void *thread_tramp(void *p)
{
    not_thread *nt = (not_thread *)p;
    (*nt->func)(nt->arg);
    return 0;
}

inline uintptr_t _beginthread(void (*func)(void *), size_t stack, void *ptr)
{
    not_thread *pt = new not_thread();
    pt->type = "this is a pthread";
    pt->arg = ptr;
    pt->func = func;
    int r = pthread_create(&pt->thr, 0, &thread_tramp, pt);
    if (r != 0) {
        delete pt;
        return 0;
    }
    return (uintptr_t)pt;
}

inline void sig_break(int)
{
}

inline void WaitForSingleObject(HANDLE h, unsigned long timeout)
{
    not_thread *pt = (not_thread *)h;
    assert(!strcmp(pt->type, "this is a pthread"));
    void *v = 0;
    signal(SIGALRM, sig_break);
    alarm(timeout / 1000 + 1);
    pthread_join(pt->thr, &v);
    signal(SIGALRM, SIG_DFL);
    alarm(0);
    delete pt;
}

#endif  //  introspection_not_win32_h
