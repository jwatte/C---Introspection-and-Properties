
#include "introspection.h"

#include <assert.h>
#include <new>


namespace introspection
{

//  size_t may be 4 or 8 bytes, but we don't support blocks with sizes bigger 
//  than what fits in 4 bytes, so use that for storage.
void write_block(size_t size, void const *data, stream &oStr)
{
    if (size > INTROSPECTION_MAX_BLOCK_SIZE)
    {
        throw std::runtime_error("block size too large");
    }
    unsigned int ui = (unsigned int)size;
    assert(sizeof(ui) == 4);
    oStr.write_bytes(sizeof(ui), &ui);
    oStr.write_bytes(ui, data);
}

void read_block_length(size_t &oLen, stream &oStr)
{
    unsigned int ui = 0;
    oStr.read_bytes(sizeof(ui), &ui);
    oLen = ui;
}

void read_block_data(size_t cnt, void *dst, stream &oStr)
{
    oStr.read_bytes(cnt, dst);
}



simple_stream::simple_stream() :
    ptr_(0),
    phys_(0),
    log_(0),
    pos_(0)
{
}

simple_stream::~simple_stream()
{
    delete[] ptr_;
}

simple_stream::simple_stream(simple_stream const &o)
{
    phys_ = (o.log_ + 31) & ~31;
    if (phys_ > 0)
    {
        ptr_ = new char[phys_];
    }
    else
    {
        ptr_ = 0;
    }
    log_ = o.log_;
    pos_ = 0;
    memcpy(ptr_, o.ptr_, log_);
}

simple_stream &simple_stream::operator=(simple_stream const &o)
{
    if (this != &o)
    {
        this->~simple_stream();
        new (this) simple_stream(o);
    }
    return *this;
}

size_t simple_stream::bytes_left()
{
    return log_ - pos_;
}

void simple_stream::read_bytes(size_t cnt, void *dst)
{
    if (cnt > bytes_left())
    {
        throw std::runtime_error("underflow in stream read_bytes()");
    }
    memcpy(dst, ptr_ + pos_, cnt);
    pos_ += cnt;
}

void simple_stream::write_bytes(size_t cnt, void const *src)
{
    if (pos_ + cnt > phys_)
    {
        size_t nPhys = phys_ + 64;      //  some linear growth, useful at the beginning
        nPhys = nPhys + (nPhys >> 1);   //  some exponential growth, without the waste of doubling
        nPhys = nPhys & ~31;            //  round the size to something nice and, uh, round.
        char *nu = new char[nPhys];
        memcpy(nu, ptr_, log_);
        phys_ = nPhys;
        delete[] ptr_;
        ptr_ = nu;
    }
    memcpy(ptr_ + pos_, src, cnt);
    pos_ += cnt;
    if (pos_ > log_)
        log_ = pos_;
}

size_t simple_stream::position()
{
    return pos_;
}

void simple_stream::set_position(size_t pos)
{
    if (pos > log_)
    {
        throw std::runtime_error("attempt to seek beyond end of stream in set_position()");
    }
    pos_ = pos;
}

void simple_stream::truncate_at_pos()
{
    log_ = pos_;
    //  accept some maximum amount of waste before reducing the size of the block
    if (phys_ > log_ * 2 && phys_ > 2048)
    {
        size_t nPhys = (log_ + 31) & ~31;
        char *nu = new char[nPhys];
        memcpy(nu, ptr_, log_);
        phys_ = nPhys;
        delete[] ptr_;
        ptr_ = nu;
    }
}



readonly_stream::readonly_stream(void const *data, size_t size) :
    ptr_(data),
    log_(size),
    pos_(0)
{
}

size_t readonly_stream::bytes_left()
{
    return log_ - pos_;
}

void readonly_stream::read_bytes(size_t cnt, void *dst)
{
    if (cnt > bytes_left())
    {
        throw std::runtime_error("underflow in stream read_bytes()");
    }
    memcpy(dst, (char const *)ptr_ + pos_, cnt);
    pos_ += cnt;
}

size_t readonly_stream::position()
{
    return pos_;
}

void readonly_stream::set_position(size_t pos)
{
    if (pos > log_)
    {
        throw std::runtime_error("attempt to set_position() beyond end of stream");
    }
    pos_ = pos;
}

}
