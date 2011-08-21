
#if !defined(introspection_sample_chat_h)
#define introspection_sample_chat_h

#include <introspection/introspection.h>

struct LoginPacket
{
    int version;
    std::string name;
    std::string password;

    INTROSPECTION(LoginPacket, \
        MEMBER(version, "version of protocol") \
        MEMBER(name, "user name") \
        MEMBER(password, "password") \
        );
};

struct UserInfo
{
    std::string name;
    std::string email;
    std::string password;
    int shoe_size;

    INTROSPECTION(UserInfo, \
        MEMBER(name, "user name") \
        MEMBER(email, "e-mail address") \
        MEMBER(password, "user password") \
        MEMBER(shoe_size, introspection::int_range("shoe size (European)", 30, 50)) \
        );
};

struct ConnectedPacket
{
    int result;
    int version;
    std::list<std::string> users;

    INTROSPECTION(ConnectedPacket, \
        MEMBER(result, "result of operation") \
        MEMBER(version, "version of protocol") \
        MEMBER(users, "connected users") \
        );
};

struct SaySomethingPacket
{
    std::string message;

    INTROSPECTION(SaySomethingPacket, \
        MEMBER(message, "what to say") \
        );
};

struct SomeoneSaidSomethingPacket
{
    std::string who;
    std::string what;

    INTROSPECTION(SomeoneSaidSomethingPacket, \
        MEMBER(who, "who said it") \
        MEMBER(what, "what they said") \
        );
};

struct UserJoinedPacket
{
    std::string who;

    INTROSPECTION(UserJoinedPacket, \
        MEMBER(who, "who joined") \
        );
};

struct UserLeftPacket
{
    std::string who;

    INTROSPECTION(UserLeftPacket, \
        MEMBER(who, "who left") \
        );
};

using namespace introspection;


#endif  //  introspection_sample_chat_h
