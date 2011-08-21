
#include "sample_chat.h"

PROTOCOL(my_proto, \
    PDU(LoginPacket) \
    PDU(UserInfo) \
    PDU(ConnectedPacket) \
    PDU(SaySomethingPacket) \
    PDU(SomeoneSaidSomethingPacket) \
    PDU(UserJoinedPacket) \
    PDU(UserLeftPacket)
    );

