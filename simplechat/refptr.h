#if !defined(simplechat_refptr_h)
#define simplechat_refptr_h

/* This generally comes from boost (shared_ptr).
   Reference count an instance of an object using an out-of-line reference count.
   It is an error to create more than one ref_ptr to the same object from the 
   base pointer -- create one, and then create copies of the first one.
   */
template<typename T>
class ref_ptr
{
    public:
        ref_ptr() :
            t_(0),
            ref_(0)
        {
        }
        ref_ptr(T *t) :
            t_(t),
            ref_(new int(1))
        {
        }
        ~ref_ptr()
        {
            if (ref_ && !--*ref_)
            {
                delete ref_;
                delete t_;
            }
        }
        ref_ptr(ref_ptr const &o)
        {
            t_ = o.t_;
            ref_ = o.ref_;
            if (ref_)
            {
                ++*ref_;
            }
        }
        ref_ptr &operator=(ref_ptr const &o)
        {
            if (&o == this) return *this;
            if (ref_ && !--*ref_)
            {
                delete ref_;
                delete t_;
            }
            t_ = o.t_;
            ref_ = o.ref_;
            if (ref_)
            {
                ++*ref_;
            }
            return *this;
        }
        operator T *() const { return t_; }
        operator T const *() const { return t_; }
        T *operator->() const { return t_; }
        T &operator*() const { return *t_; }
        bool operator!() const { return !t_; }
        T *t_;
        int *ref_;
};


#endif  //  simplechat_refptr_h
