
#include "sample_chat.h"
#include <assert.h>
#include <sstream>
#include <iostream>

void test_basic_marshal()
{
    LoginPacket lp;
    lp.version = 1;
    lp.name = "Jon Watte";
    lp.password = "super secret";
    std::string ostr;
    lp.member_info().access().to_text(&lp, ostr);
    assert(ostr == std::string("[ 1 \"Jon Watte\" \"super secret\" ] "));

    LoginPacket lp2;
    char const *rest = lp2.member_info().access().from_text(&lp2, ostr.c_str());
    assert(lp.version == lp2.version);
    assert(lp.name == lp2.name);
    assert(lp.password == lp2.password);

    simple_stream ss;
    LoginPacket lp3;
    LoginPacket::member_info().access().get_from(&lp2, ss);
    ss.set_position(0);
    LoginPacket::member_info().access().put_to(&lp3, ss);
    assert(lp.version == lp3.version);
    assert(lp.name == lp3.name);
    assert(lp.password == lp3.password);

    ConnectedPacket cp;
    cp.result = 10;
    cp.version = 20;
    cp.users.push_back("The First User");
    cp.users.push_back("Some Second User");
    cp.users.push_back("Operator");
    ConnectedPacket::member_info().access().to_text(&cp, ostr);
    assert(ostr == std::string("[ 10 20 { \"The First User\" \"Some Second User\" \"Operator\" } ] "));
    ConnectedPacket cp2;
    ConnectedPacket::member_info().access().from_text(&cp2, ostr.c_str());
    assert(cp.result == cp2.result);
    assert(cp.version == cp2.version);
    assert(cp.users.size() == cp2.users.size());
    assert(cp.users == cp2.users);
}

void test_introspection()
{
    std::stringstream ss;
    type_info_base const &tib = UserInfo::member_info();
    for (member_t::iterator ptr(tib.begin()), end(tib.end()); ptr != end; ++ptr)
    {
        ss << "member: " << (*ptr).name() << " desc: " << (*ptr).info().desc() <<
            " offset: " << (*ptr).access().offset() << " size: " << (*ptr).access().size();
        if ((*ptr).access().compound()) {
            ss << " [compound]";
        }
        if ((*ptr).access().collection()) {
            ss << " {collection}";
        }
        ss << std::endl;
        std::cout << ss.str();
        ss.str("");
    }
}

EXTERN_PROTOCOL(my_proto);

class MyHandler
{
    public:
        MyHandler() :
            called_(false)
        {
        }
        bool called_;
        void OnLoginPacket(LoginPacket const  &lp)
        {
            called_ = true;
            assert(lp.name == "My Name");
            assert(lp.password == "123qwe");
            assert(lp.version == 1);
        }
        void OnConnectedPacket(ConnectedPacket const  &cp)
        {
            called_ = true;
            assert(cp.version == 1);
            assert(cp.result == 0);
            assert(cp.users.size() == 3);
            std::list<std::string>::const_iterator ptr(cp.users.begin());
            assert(*ptr++ == "Administrator");
            assert(*ptr++ == "Another User");
            assert(*ptr++ == "User 1");
            assert(ptr == cp.users.end());
        }
};

/* an instance -- you might create one per connected user, say */
MyHandler my_handler;

void test_protocol()
{
    //  sending side
    simple_stream ss;
    LoginPacket lp;
    lp.name = "My Name";
    lp.password = "123qwe";
    lp.version = 1;
    //  in the protocol, this would come in the other direction, but 
    //  test marshaling by jamming them in the same stream packet
    ConnectedPacket cp;
    cp.version = 1;
    cp.result = 0;
    cp.users.push_back("Administrator");
    cp.users.push_back("Another User");
    cp.users.push_back("User 1");
    my_proto.encode(lp, ss);    //  add the loginpacket
    my_proto.encode(cp, ss);    //  add the connectedpacket

    //  receiving side
    dispatch_t d;
    d.add_handler(my_proto, &my_handler, &MyHandler::OnLoginPacket);
    d.add_handler(my_proto, &my_handler, &MyHandler::OnConnectedPacket);

    ss.set_position(0);
    char buf[1024];

    //  read the first packet
    int i = my_proto.decode(buf, sizeof(buf), ss);
    //  dispatch based on "i"
    d.dispatch(i, buf);
    assert(my_handler.called_);
    my_handler.called_ = false;
    my_proto.destroy(i, buf);   //  restore memory

    //  read the second packet
    i = my_proto.decode(buf, sizeof(buf), ss);
    d.dispatch(i, buf);
    assert(my_handler.called_);
    my_handler.called_ = false;
    my_proto.destroy(i, buf);
}


int main(int argc, char const *argv[])
{
    test_basic_marshal();
    test_introspection();
    test_protocol();
    return 0;
}
