
#if !defined(introspection_introspection_h)
#define introspection_introspection_h

#include <vector>
#include <list>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <ctype.h>
#include <string.h>
#include <stdexcept>

#define INTROSPECTION_MAX_BLOCK_SIZE (32*1024*1024)

namespace introspection
{
    struct member_t;
    struct member_access_base;
    #define THROW_EXCEPTION(x) \
        struct x : std::exception {}; \
        throw x()

    #define INTROSPECTION(type, members) \
        typedef type self_t; \
        static inline introspection::type_info_base const &member_info() { \
            static introspection::member_t data[] = { \
                members \
            }; \
            static introspection::type_info_t<type> info( \
                data, sizeof(data)/sizeof(data[0]), \
                introspection::struct_access_t<type>::instance()); \
            return info; \
        }
    #define MEMBER(name, desc) \
        introspection::member_instance(&self_t::name, #name, desc),

    #define EXTERN_PROTOCOL(name) \
        extern protocol_t name

    #define PROTOCOL(name, pdus) \
        protocol_t name(protocol_t(#name) \
            pdus \
            )
    #define PDU(type) \
        .add_pdu<type>()


    /* binary marshaling support */
    /* n.b. if you want to save some bytes, you probably want to encode unsigned */
    /* integers as base-128 var-length ints, and signed integers as zig-zag encoded */
    /* base-128 var-length ints. */

    struct stream
    {
        virtual size_t bytes_left() = 0;
        virtual void read_bytes(size_t cnt, void *dst) = 0;
        virtual void write_bytes(size_t cnt, void const *src) = 0;
        virtual size_t position() = 0;
        virtual void set_position(size_t pos) = 0;
    };
    void write_block(size_t len, void const *data, stream &oStr);
    void read_block_length(size_t &len, stream &iStr);
    void read_block_data(size_t len, void *data, stream &iStr);

    template<typename T, bool HasMemberInfo> struct marshal;
    template<typename T, bool HasMemberInfo> struct convert;

    inline static void quote_str(char const *iStr, std::string &oStr)
    {
        oStr.push_back('\"');
        while (*iStr)
        {
            if (*iStr == '\"' || *iStr == '\\')
            {
                oStr.push_back('\\');
            }
            oStr.push_back(*iStr);
            ++iStr;
        }
        oStr.push_back('\"');
    }

    inline static char const *unquote_str(char const *iStr, std::string &oStr)
    {
        if (*iStr != '\"')
        {
            throw std::runtime_error("badly formatted string in unquote_str (not quoted)");
        }
        ++iStr;
        while (true)
        {
            if (*iStr == 0)
            {
                throw std::runtime_error("badly formatted string in unquote_str (early end)");
            }
            else if (*iStr == '\\')
            {
                if (iStr[1] == 0)
                {
                    throw std::runtime_error("badly formatted string in unquote_str (quoted nul)");
                }
                ++iStr;
                oStr.push_back(*iStr);
            }
            else if (*iStr == '\"')
            {
                ++iStr;
                break;
            }
            else
            {
                oStr.push_back(*iStr);
            }
            ++iStr;
        }
        return iStr;
    }


    /* introspection support */

    struct collection_info_base
    {
        virtual size_t size(void const *coll) const = 0;
        virtual bool begin_iteration(void const *coll, void *&oPtr, void *&oEnd, member_access_base *&oAccess) const = 0;
        virtual void get_element(void *iPtr, void *&oMem) const = 0;
        virtual bool increment(void *&iPtr, void *&iEnd) const = 0;
        virtual void cleanup(void *&iPtr, void *&iEnd) const = 0;
        virtual void clear(void *coll) const = 0;
        virtual void append_from(void *coll, stream &iStr) const = 0;
        virtual char const *append_from(void *coll, char const *str) const = 0;
    };

    /* basic information about an aggregate type (struct) */
    struct type_info_base
    {
        type_info_base(member_t const *ptr, size_t cnt, member_access_base const &access) :
            members_(ptr),
            count_(cnt),
            access_(access)
        {
        }
        inline member_t const  *begin() const;
        inline member_t const  *end() const;
        inline member_access_base const &access() const { return access_; }
    protected:
        member_t const         *members_;
        size_t                  count_;
        member_access_base const &access_;
    };

    /* information about a specific type (creation, destruction, marshaling) */
    struct member_access_base
    {
        member_access_base(size_t mem_size, size_t offset, type_info_base const *base, collection_info_base const *collection) :
            mem_size_(mem_size),
            offset_(offset),
            base_(base),
            collection_(collection)
        {
        }
        inline void get_from(void const *strct, stream &oStr) const;
        inline void put_to(void *strct, stream &iStr) const;
        inline void to_text(void const *strct, std::string &oStr) const;
        inline char const *from_text(void *strct, char const *str) const;
        inline size_t size() const { return mem_size_; }
        inline size_t offset() const { return offset_; }
        inline bool compound() const { return base_ != 0; }
        inline type_info_base const &member_info() const { return *base_; }
        inline bool collection() const { return collection_ != 0; }
        inline collection_info_base const &collection_info() const { return *collection_; }
        virtual void create(void *ptr) const = 0;
        virtual void destroy(void *ptr) const = 0;
    private:
        virtual void do_get_from(void const *strct, stream &oStr) const = 0;
        virtual void do_put_to(void *strct, stream &iStr) const = 0;
        virtual void do_to_text(void const *strct, std::string &ostr) const = 0;
        virtual char const *do_from_text(void *strct, char const *str) const = 0;
        size_t mem_size_;
        size_t offset_;
        type_info_base const *base_;
        collection_info_base const *collection_;
    };

    struct member_info_base
    {
        member_info_base(char const *desc) :
            desc_(desc)
        {
        }
        inline char const *desc() const { return desc_; }
    protected:
        char const *desc_;
    };

    /* a member_t is a member of a compound structure. It "knows" about its membership, 
       so it "knows" where in the structure it lives.
       */
    struct member_t
    {
        member_t(
            char const *name,
            member_access_base &access,
            member_info_base &info) :
            name_(name),
            access_(access),
            info_(info)
        {
        }
        typedef member_t const *iterator;
        inline char const *name() const { return name_; }
        inline member_access_base const &access() const { return access_; }
        inline member_info_base const &info() const { return info_; }
    protected:
        char const *const name_;
        member_access_base &access_;
        member_info_base &info_;
    };

    template<typename Member>
    struct type_info_t : type_info_base
    {
        type_info_t(member_t const *ptr, size_t cnt, member_access_base const &mab) :
            type_info_base(ptr, cnt, mab)
        {
        }
    };

    template<typename T> struct has_member_info
    {
        template<int N>
        struct bar {
            char x[N];
        };
        template<typename Q>
        static inline char sfinae(Q *t, bar<sizeof(&Q::member_info)> *u) { return sizeof(*u); }
        static inline int sfinae(...) { return 4; }
        enum { value = (1 == sizeof(sfinae((T *)0))) };
    };

    template<typename MemT, bool HasMemberInfo> struct get_member_info_base;
    template<typename MemT> struct get_member_info_base<MemT, false>
    {
        static inline type_info_base const *info() { return 0; }
    };
    template<typename MemT> struct get_member_info_base<MemT, true>
    {
        static inline type_info_base const *info() { return &MemT::member_info(); }
    };
    template<typename MemT> struct get_member_info : get_member_info_base<MemT, has_member_info<MemT>::value>
    {
    };

    template<typename MemT> struct get_collection_info
    {
        enum { is_collection = 0 };
        static inline collection_info_base const *info() { return 0; }
    };
    template<typename Coll>
    struct collection_t : collection_info_base
    {
        inline static collection_t &instance()
        {
            static collection_t it;
            return it;
        }
        virtual size_t size(void const *coll) const
        {
            return ((Coll const *)coll)->size();
        }
        template<typename MemT>
        struct IterDeref : member_access_base
        {
            IterDeref() :
                member_access_base(
                    sizeof(MemT),
                    0, 
                    get_member_info<MemT>::info(),
                    get_collection_info<MemT>::info())
            {
            }
            virtual void create(void *ptr) const
            {
                new (ptr) MemT;
            }
            virtual void destroy(void *ptr) const
            {
                ((MemT *)ptr)->~MemT();
            }
            virtual void do_get_from(void const *strct, stream &oStr) const
            {
                marshal<MemT, has_member_info<MemT>::value>::output(*(MemT const *)strct, oStr);
            }
            virtual void do_put_to(void *strct, stream &iStr) const
            {
                marshal<MemT, has_member_info<MemT>::value>::input(*(MemT *)strct, iStr);
            }
            virtual void do_to_text(void const *strct, std::string &oStr) const
            {
                convert<MemT, has_member_info<MemT>::value>::to_string(*(MemT const *)strct, oStr);
            }
            virtual char const *do_from_text(void *strct, char const *str) const
            {
                return convert<MemT, has_member_info<MemT>::value>::from_string(*(MemT *)strct, str);
            }
        };
        static inline member_access_base &get_access()
        {
            static IterDeref<typename Coll::value_type> access;
            return access;
        }
        /* begin iteration of a collection. Make sure to call cleanup() when 
           you are done! (This is called for you by increment() if it gets to the end.) */
        virtual bool begin_iteration(void const *coll, void *&oPtr, void *&oEnd, member_access_base *&oAccess) const
        {
            oAccess = &get_access();
            Coll const &c = *(Coll const *)coll;
            typename Coll::const_iterator ptr(c.begin());
            typename Coll::const_iterator end(c.end());
            if (ptr == end)
            {
                oPtr = 0;
                oEnd = 0;
                return false;
            }
            oPtr = new typename Coll::const_iterator(c.begin());
            oEnd = new typename Coll::const_iterator(c.end());
            return true;
        }
        virtual void get_element(void *iPtr, void *&oMem) const
        {
            oMem = const_cast<void *>(static_cast<void const *>(&**(typename Coll::const_iterator *)iPtr));
        }
        virtual bool increment(void *&iPtr, void *&iEnd) const
        {
            if (iPtr == 0 || iEnd == 0)
            {
                return false;
            }
            typename Coll::const_iterator &ptr(*(typename Coll::const_iterator *)iPtr);
            typename Coll::const_iterator &end(*(typename Coll::const_iterator *)iEnd);
            ++ptr;
            if (ptr == end)
            {
                cleanup(iPtr, iEnd);
                return false;
            }
            return true;
        }
        virtual void cleanup(void *&iPtr, void *&iEnd) const
        {
            if (iPtr != 0)
            {
                delete (typename Coll::const_iterator *)iPtr;
                iPtr = 0;
            }
            if (iEnd != 0)
            {
                delete (typename Coll::const_iterator *)iEnd;
                iEnd = 0;
            }
        }
        virtual void clear(void *coll) const
        {
            ((Coll *)coll)->clear();
        }
        template<typename T>
        struct insert
        {
            static inline void func(void *coll, typename T::value_type const &vt)
            {
                (*(Coll *)coll).push_back(vt);
            }
        };
        template<typename T>
        struct insert<std::set<T> >
        {
            static inline void func(void *coll, typename T::value_type const &vt)
            {
                (*(std::set<T> *)coll).insert(vt);
            }
        };
        virtual void append_from(void *coll, stream &iStr) const
        {
            typename Coll::value_type tmp;
            get_access().put_to(&tmp, iStr);
            insert<Coll>::func(coll, tmp);
        }
        virtual char const *append_from(void *coll, char const *str) const
        {
            typename Coll::value_type tmp;
            str = get_access().from_text(&tmp, str);
            insert<Coll>::func(coll, tmp);
            return str;
        }
    };
    template<typename MemT> struct get_collection_info<std::list<MemT> >
    {
        enum { is_collection = 1 };
        static inline collection_info_base const *info() {
            return &collection_t<std::list<MemT> >::instance();
        }
    };
    template<typename MemT> struct get_collection_info<std::vector<MemT> >
    {
        enum { is_collection = 1 };
        static inline collection_info_base const *info() {
            return &collection_t<std::vector<MemT> >::instance();
        }
    };
    template<typename MemT> struct get_collection_info<std::set<MemT> >
    {
        enum { is_collection = 1 };
        static inline collection_info_base const *info() {
            return &collection_t<std::set<MemT> >::instance();
        }
    };

    template<typename Struct, typename MemT, bool IsCollection> struct member_access_t;
    template<typename Struct, typename MemT>
    struct member_access_t<Struct, MemT, false> : member_access_base
    {
        member_access_t(MemT Struct::*member) :
            member_access_base(
                sizeof(MemT), 
                (char *)&(((Struct *)0)->*member) - (char *)0,
                get_member_info<MemT>::info(),
                get_collection_info<MemT>::info()),
            member_(member)
        {
        }
        virtual void create(void *ptr) const
        {
            new (ptr) MemT;
        }
        virtual void destroy(void *ptr) const
        {
            ((MemT *)ptr)->~MemT();
        }
        virtual void do_get_from(void const *strct, stream &oStr) const
        {
            marshal<MemT, has_member_info<MemT>::value>::output(((Struct const *)strct)->*member_, oStr);
        }
        virtual void do_put_to(void *strct, stream &iStr) const
        {
            marshal<MemT, has_member_info<MemT>::value>::input(((Struct *)strct)->*member_, iStr);
        }
        virtual void do_to_text(void const *strct, std::string &oStr) const
        {
            convert<MemT, has_member_info<MemT>::value>::to_string(((Struct const *)strct)->*member_, oStr);
        }
        virtual char const *do_from_text(void *strct, char const *str) const
        {
            return convert<MemT, has_member_info<MemT>::value>::from_string(((Struct *)strct)->*member_, str);
        }
        inline MemT Struct::*member() const { return member_; }
    protected:
        MemT Struct::*member_;
    };
    template<typename Struct, typename MemT>
    struct member_access_t<Struct, MemT, true> : member_access_base
    {
        member_access_t(MemT Struct::*member) :
            member_access_base(
                sizeof(MemT), 
                (char *)&(((Struct *)0)->*member) - (char *)0,
                get_member_info<MemT>::info(),
                get_collection_info<MemT>::info()),
            member_(member)
        {
        }
        virtual void create(void *ptr) const
        {
            new (ptr) MemT;
        }
        virtual void destroy(void *ptr) const
        {
            ((MemT *)ptr)->~MemT();
        }
        virtual void do_get_from(void const *strct, stream &oStr) const
        {
            throw std::logic_error("do_get_from() on struct");
        }
        virtual void do_put_to(void *strct, stream &iStr) const
        {
            throw std::logic_error("do_put_to() on struct");
        }
        virtual void do_to_text(void const *strct, std::string &oStr) const
        {
            throw std::logic_error("do_to_text() on struct");
        }
        virtual char const *do_from_text(void *strct, char const *str) const
        {
            throw std::logic_error("do_from_text() on struct");
        }
        inline MemT Struct::*member() const { return member_; }
    protected:
        MemT Struct::*member_;
    };
    template<typename MemT>
    struct struct_access_t : member_access_base
    {
        static inline struct_access_t const &instance()
        {
            static struct_access_t access;
            return access;
        }
        struct_access_t() :
            member_access_base(
                sizeof(MemT), 
                0,
                get_member_info<MemT>::info(),
                0)
        {
        }
        virtual void create(void *ptr) const
        {
            new (ptr) MemT;
        }
        virtual void destroy(void *ptr) const
        {
            ((MemT *)ptr)->~MemT();
        }
        virtual void do_get_from(void const *strct, stream &oStr) const
        {
            marshal<MemT, true>::output(*(MemT const *)strct, oStr);
        }
        virtual void do_put_to(void *strct, stream &iStr) const
        {
            marshal<MemT, true>::input(*(MemT *)strct, iStr);
        }
        virtual void do_to_text(void const *strct, std::string &oStr) const
        {
            convert<MemT, true>::to_string(*(MemT const *)strct, oStr);
        }
        virtual char const *do_from_text(void *strct, char const *str) const
        {
            return convert<MemT, true>::from_string(*(MemT *)strct, str);
        }
    };

    /* raw pointers are not supported, because memory management becomes a problem */
    template<typename Struct, typename Item> struct member_access_t<Struct, Item const *, false>;
    template<typename Struct, typename Item> struct member_access_t<Struct, Item *, false>;

    //  Specialize get_desc() on your description if it's custom
    template<typename Desc> char const *get_desc(Desc const &str) { return Desc::get_desc(str); }
    //  non-template overrides template
    inline char const *get_desc(char const *const &str) { return str; }
    inline char const *get_desc(std::string const &str) { return str.c_str(); }

    template<typename Desc>
    struct member_info_t : member_info_base
    {
        member_info_t(Desc const &desc)
            : member_info_base(get_desc(desc))
        {
        }
    };

    template<typename Desc, typename Struct, typename MemT>
    member_info_base *member_info(Desc const &desc)
    {
        return new member_info_t<Desc>(desc);
    }

    template<typename Struct, typename MemT, typename Desc>
    member_t member_instance(MemT Struct::*member, char const *name, Desc const &desc)
    {
        return member_t(
            name,
            *new member_access_t<Struct, MemT, get_collection_info<MemT>::is_collection>(member),
            *member_info<Desc, Struct, MemT>(desc));
    }

    template<typename T>
    struct range
    {
        range(char const *text, T const &low, T const &high) :
            text_(text),
            low_(low),
            high_(high)
        {
        }
        char const *text_;
        T low_;
        T high_;
        static inline char const *get_desc(range const &r) { return r.text_; }
    };
    typedef range<int> int_range;

    inline member_t const *type_info_base::begin() const { return members_; }
    inline member_t const *type_info_base::end() const { return members_ + count_; }

    /* simple_stream will reallocate its chunk of memory to it whatever you try to write 
       into it. It's a good first order approximation for how to set up the output part 
       of a file writer or network stream.
       */
    struct simple_stream : stream
    {
        simple_stream();
        ~simple_stream();
        simple_stream(simple_stream const &o);
        simple_stream &operator=(simple_stream const &o);
        virtual size_t bytes_left();
        virtual void read_bytes(size_t cnt, void *dst);
        virtual void write_bytes(size_t cnt, void const *src);
        virtual size_t position();
        virtual void set_position(size_t pos);
        void truncate_at_pos();
        void *unsafe_data() { return ptr_; }
    private:
        char *ptr_;
        size_t phys_;
        size_t log_;
        size_t pos_;
    };

    /* readonly_stream makes it possible to de-marshal data from a user buffer */
    struct readonly_stream : stream
    {
        readonly_stream(void const *data, size_t size);
        virtual size_t bytes_left();
        virtual void read_bytes(size_t cnt, void *dst);
        virtual void write_bytes(size_t cnt, void const *src)
            {
                throw std::logic_error("can't write to a readonly stream");
            }
        virtual size_t position();
        virtual void set_position(size_t pos);
        void truncate_at_pos()
            {
                throw std::logic_error("can't truncate a readonly stream");
            }
    private:
        void const *ptr_;
        size_t log_;
        size_t pos_;
    };

    /* A protocol is a collection of marshalable packets, each of which is given 
       an identifier (integer) to make packing/unpacking to/from a stream possible. */
    struct protocol_t
    {
        protocol_t(char const *name);
        protocol_t(protocol_t const &proto);
        protocol_t &operator=(protocol_t const &proto);

        /* name of the protocol */
        char const *name() const;

        /* used by macros declaring PDUs for the protocol */
        template<typename Pdu>
        inline protocol_t &add_pdu();

        /* code for a given PDU */
        template<typename Pdu>
        inline int code() const;

        /* type for a given PDU code */
        inline type_info_base const &type(int code);

        /* Encode a given concrete PDU into a stream for later decoding.
         */
        template<typename Pdu>
        void encode(Pdu const &t, stream &s);

        /* Decode a PDU in a given stream into the memory given. This 
         * will instantiate the appropriate concrete class, assuming 
         * sizeof(TheClass) is <= max_size.
         */
        inline int decode(void *dst, size_t max_size, stream &s);

        /* call the right destructor for the given packet code */
        inline void destroy(int code, void *dst);

    private:
        protocol_t &add_pdu(type_info_base const *pdu);
        std::map<int, type_info_base const *> by_id_;
        std::map<type_info_base const *, int> by_type_;
        std::string name_;
    };

    class dispatch_t
    {
        public:
            dispatch_t();
            virtual ~dispatch_t();

            template<typename Pdu, typename Handler>
            void add_handler(protocol_t const &proto, Handler *handler, void (Handler::*func)(Pdu const &))
            {
                dispatch_[proto.code<Pdu>()] = new dispatch_rec<Pdu, Handler>(handler, func);
            }

            void dispatch(int c, void const *data);

        private:
            class dispatch_base
            {
                public:
                    virtual void dispatch(void const *ptr) = 0;
            };
 
            dispatch_t(dispatch_t const &);
            dispatch_t &operator=(dispatch_t const &);
            std::map<int, dispatch_base *> dispatch_;
 
            template<typename Pdu, typename Handler>
            class dispatch_rec : public dispatch_base
            {
                public:
                    dispatch_rec(Handler *h, void (Handler::*func)(Pdu const &)) :
                        handler_(h),
                        func_(func)
                    {
                    }
                    virtual void dispatch(void const *ptr)
                    {
                        (handler_->*func_)(*(Pdu const *)ptr);
                    }
                private:
                    Handler *handler_;
                    void (Handler::*func_)(Pdu const &);
            };

    };

    template<typename T>
    struct marshal<T, false>
    {
        inline static void output(T const &item, stream &oStr)
        {
            oStr.write_bytes(sizeof(T), &item);
        }
        inline static void input(T &item, stream &iStr)
        {
            iStr.read_bytes(sizeof(T), &item);
        }
    };
    template<>
    struct marshal<std::string, false>
    {
        inline static void output(std::string const &item, stream &oStr)
        {
            write_block(item.length(), item.c_str(), oStr);
        }
        inline static void input(std::string &item, stream &iStr)
        {
            size_t len = 0;
            read_block_length(len, iStr);
            item.resize(len);
            if (len > 0)
                read_block_data(len, &item[0], iStr);
        }
    };
    template<>
    struct marshal<char const *, false>
    {
        inline static void output(char const *const &item, stream &oStr)
        {
            write_block(strlen(item), item, oStr);
        }
        // inline static void input(char const *&item, stream &oStr) // this doesn't!
    };
    template<typename MemT>
    struct marshal<MemT, true>
    {
        inline static void output(MemT const &item, stream &oStr)
        {
            for (member_t::iterator ptr(item.member_info().begin()), end(item.member_info().end());
                ptr != end; ++ptr)
            {
                (*ptr).access().get_from(&item, oStr);
            }
        }
        inline static void input(MemT &item, stream &iStr)
        {
            for (member_t::iterator ptr(item.member_info().begin()), end(item.member_info().end());
                ptr != end; ++ptr)
            {
                (*ptr).access().put_to(&item, iStr);
            }
        }
    };

    /* text conversion support */

    template<typename T>
    struct convert<T, false>
    {
        inline static void to_string(T const &item, std::string &oStr)
        {
            std::stringstream strm;
            strm << item;
            strm >> oStr;
            oStr += " ";
        }
        inline static char const *from_string(T &item, char const *str)
        {
            while (*str && isspace(*str))
            {
                ++str;
            }
            if (!*str)
            {
                throw std::logic_error("underflow in scalar from_string()");
            }
            char const *end = str;
            while (*end && !isspace(*end))
            {
                ++end;
            }
            std::stringstream strm;
            strm << std::string(str, end);
            strm >> item;
            return end;
        }
    };

    template<>
    struct convert<std::string, false>
    {
        inline static void to_string(std::string const &item, std::string &oStr)
        {
            oStr.clear();
            quote_str(item.c_str(), oStr);
            oStr += " ";
        }
        inline static char const *from_string(std::string &item, char const *iStr)
        {
            item.clear();
            return unquote_str(iStr, item);
        }
    };
    template<>
    struct convert<char const *, false>
    {
        inline static void to_string(char const * const &item, std::string &oStr)
        {
            quote_str(item, oStr);
            oStr += " ";
        }
        //  void from_string(char const *&item, char const *iStr)   //  no can do
    };

    template<typename MemT>
    struct convert<MemT, true>
    {
        inline static void to_string(MemT const &item, std::string &oStr)
        {
            oStr = "[ ";
            for (member_t::iterator ptr(item.member_info().begin()), end(item.member_info().end());
                ptr != end; ++ptr)
            {
                std::string tmp;
                (*ptr).access().to_text(&item, tmp);
                oStr += tmp;
            }
            oStr += "] ";
        }
        inline static char const *from_string(MemT &item, char const *iStr)
        {
            while (*iStr && isspace(*iStr))
            {
                ++iStr;
            }
            if (!*iStr)
            {
                throw std::runtime_error("underflow before structure from_string()");
            }
            if (*iStr != '[')
            {
                throw std::runtime_error("missing bracket in structure from_string()");
            }
            ++iStr;
            for (member_t::iterator ptr(item.member_info().begin()), end(item.member_info().end());
                ptr != end; ++ptr)
            {
                while (*iStr && isspace(*iStr))
                {
                    ++iStr;
                }
                if (!*iStr)
                {
                    throw std::runtime_error("underflow in structure from_string()");
                }
                iStr = (*ptr).access().from_text(&item, iStr);
            }
            while (*iStr && isspace(*iStr))
            {
                ++iStr;
            }
            if (*iStr != ']')
            {
                //  todo: skip "future" unknown fields here, for backwards compatibility
                throw std::runtime_error("missing end bracket in structure from_string()");
            }
            ++iStr;
            return iStr;
        }
    };

    inline void member_access_base::get_from(void const *strct, stream &oStr) const 
    {
        if (collection_)
        {
            unsigned int cnt = collection_->size((char const *)strct + offset_);
            marshal<unsigned int, false>::output(cnt, oStr);
            void *ptr = 0, *end = 0;
            member_access_base *acc;
            try
            {
                if (collection_->begin_iteration((char const *)strct + offset_, ptr, end, acc)) do
                {
                    void *elem;
                    collection_->get_element(ptr, elem);
                    acc->get_from(elem, oStr);
                }
                while (collection_->increment(ptr, end));
            }
            catch (...)
            {
                collection_->cleanup(ptr, end);
            }
        }
        else
        {
            do_get_from(strct, oStr);
        }
    }
    inline void member_access_base::put_to(void *strct, stream &iStr) const 
    {
        if (collection_)
        {
            unsigned int cnt = 0;
            marshal<unsigned int, false>::input(cnt, iStr);
            for (unsigned int i = 0; i != cnt; ++i)
            {
                collection_->append_from((char *)strct + offset_, iStr);
            }
        }
        else
        {
            do_put_to(strct, iStr);
        }
    }
    inline void member_access_base::to_text(void const *strct, std::string &oStr) const
    {
        if (collection_)
        {
            oStr = "{ ";
            void *ptr = 0, *end = 0;
            member_access_base *acc;
            try
            {
                if (collection_->begin_iteration((char const *)strct + offset_, ptr, end, acc)) do
                {
                    void *elem;
                    collection_->get_element(ptr, elem);
                    std::string tmp;
                    acc->to_text(elem, tmp);
                    oStr += tmp;
                }
                while (collection_->increment(ptr, end));
                oStr += "} ";
            }
            catch (...)
            {
                collection_->cleanup(ptr, end);
            }
        }
        else
        {
            do_to_text(strct, oStr);
        }
    }
    inline char const *member_access_base::from_text(void *strct, char const *str) const 
    {
        while (*str && isspace(*str))
        {
            ++str;
        }
        if (collection_)
        {
            if (*str != '{')
            {
                throw std::runtime_error("bad format for collection in from_text (no open brace)");
            }
            ++str;
            while (true)
            {
                while (*str && isspace(*str))
                {
                    ++str;
                }
                if (!*str)
                {
                    throw std::runtime_error("early input data end in from_text");
                }
                if (*str == '}')
                {
                    ++str;
                    break;
                }
                str = collection_->append_from((char *)strct + offset_, str);
            }
            return str;
        }
        else
        {
            return do_from_text(strct, str);
        }
    }

    /* used by macros declaring PDUs for the protocol */
    template<typename Pdu>
    inline protocol_t &protocol_t::add_pdu()
    {
        return add_pdu(&Pdu::member_info());
    }

    /* code for a given PDU */
    template<typename Pdu>
    inline int protocol_t::code() const
    {
        std::map<type_info_base const *, int>::const_iterator ptr(by_type_.find(&Pdu::member_info()));
        if (ptr == by_type_.end())
        {
            throw std::runtime_error("could not find code for type - is it registered?");
        }
        return (*ptr).second;
    }

    /* type for a given PDU code */
    inline type_info_base const &protocol_t::type(int code)
    {
        std::map<int, type_info_base const *>::iterator ptr(by_id_.find(code));
        if (ptr == by_id_.end())
        {
            throw std::runtime_error("could not find type for code - is it registered?");
        }
        return *(*ptr).second;
    }

    template<typename Pdu>
    void protocol_t::encode(Pdu const &t, stream &s)
    {
        int c = code<Pdu>();
        marshal<int, false>::output(c, s);
        Pdu::member_info().access().get_from(&t, s);
    }

    inline int protocol_t::decode(void *dst, size_t max_size, stream &s)
    {
        int c;
        marshal<int, false>::input(c, s);
        type_info_base const &t = type(c);
        if (t.access().size() > max_size)
        {
            throw std::runtime_error("not enough space for type in decode()");
        }
        t.access().create(dst);
        t.access().put_to(dst, s);
        return c;
    }

    inline void protocol_t::destroy(int code, void *dst)
    {
        type_info_base const &t = type(code);
        t.access().destroy(dst);
    }

}

#endif  //  introspection_introspection_h
