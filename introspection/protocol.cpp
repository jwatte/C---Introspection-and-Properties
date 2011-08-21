
#include <introspection/introspection.h>

namespace introspection
{

protocol_t::protocol_t(char const *name) :
    name_(name)
{
}

protocol_t::protocol_t(protocol_t const &proto) :
    by_id_(proto.by_id_),
    by_type_(proto.by_type_),
    name_(proto.name_)
{
}

protocol_t &protocol_t::operator=(protocol_t const &proto)
{
    by_id_ = proto.by_id_;
    by_type_ = proto.by_type_;
    name_ = proto.name_;
    return *this;
}

char const *protocol_t::name() const
{
    return name_.c_str();
}

protocol_t &protocol_t::add_pdu(type_info_base const *pdu)
{
    int code = by_id_.size() + 1;
    by_id_[code] = pdu;
    by_type_[pdu] = code;
    return *this;
}

dispatch_t::dispatch_t()
{
}

dispatch_t::~dispatch_t()
{
    for (std::map<int, dispatch_base *>::iterator ptr(dispatch_.begin()), end(dispatch_.end());
        ptr != end; ++ptr)
    {
        delete (*ptr).second;
    }
}

void dispatch_t::dispatch(int c, void const *data)
{
    std::map<int, dispatch_base *>::iterator ptr(dispatch_.find(c));
    if (ptr == dispatch_.end())
    {
        throw std::runtime_error("attempt to dispatch a code that wasn't registered");
    }
    (*ptr).second->dispatch(data);
}


}
