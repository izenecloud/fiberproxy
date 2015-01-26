#ifndef FIBP_MULTI_THREAD_OBJ_MGR_H
#define FIBP_MULTI_THREAD_OBJ_MGR_H

#include <util/singleton.h>
#include <util/hashFunction.h>
#include <3rdparty/folly/RWSpinLock.h>
#include <string>
#include <vector>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <sstream>

namespace fibp
{
template <class ObjectType>
class MultiThreadObjMgr
{
public:
    typedef MultiThreadObjMgr<ObjectType> this_type;
    static MultiThreadObjMgr* get()
    {
        return izenelib::util::Singleton<this_type>::get();
    }

    void init(std::size_t num)
    {
        obj_list_.resize(cal_next_prime(num*2));
        for(std::size_t i = 0; i < obj_list_.size(); ++i)
        {
            obj_list_[i].reset(new ObjectItem());
        }
    }

    MultiThreadObjMgr()
    {
    }

    ObjectType& getThreadObj()
    {
        std::size_t bucket_index = getBucketIndex(obj_list_.size());
        typename ObjectItem::ItemRWLock::WriteHolder guard(obj_list_[bucket_index]->rw_lock);
        ObjectItem& item = *(obj_list_[bucket_index]);
        for(typename ObjectItem::ItemListT::iterator it = item.item_list.begin();
            it != item.item_list.end(); ++it)
        {
            if (it->first == boost::this_thread::get_id())
            {
                return *(it->second);
            }
        }
        boost::shared_ptr<ObjectType> l;
        l.reset(new ObjectType());
        item.item_list.push_back(std::make_pair(boost::this_thread::get_id(), l));
        return *l;
    }

    void clear()
    {
        obj_list_.clear();
    }

private:

    static std::size_t getBucketIndex(std::size_t total)
    {
        ostringstream oss;
        oss << boost::this_thread::get_id();
        static izenelib::util::HashIDTraits<std::string, std::size_t> hash_func;
        return hash_func(oss.str()) % total;
    }

    enum { PRIME_NUM = 28 };
    static const int64_t PRIME_LIST[PRIME_NUM];

    inline int64_t cal_next_prime(int64_t n)
    {
        int64_t ret = n;
        if (n > 0)
        {
            const int64_t* first = PRIME_LIST;
            const int64_t* last = PRIME_LIST + PRIME_NUM;
            const int64_t* pos = std::lower_bound(first, last, n);
            ret = ((pos == last) ? *(last - 1) : *pos);
        }
        return ret;
    }

    struct ObjectItem
    {
        typedef std::list<std::pair<boost::thread::id, boost::shared_ptr<ObjectType> > > ItemListT;
        typedef folly::RWTicketSpinLockT<32, true> ItemRWLock;
        ItemRWLock rw_lock;
        ItemListT item_list;
    };

    std::vector<boost::shared_ptr<ObjectItem> > obj_list_;
};

template <class ObjectType>
const int64_t MultiThreadObjMgr<ObjectType>::PRIME_LIST[MultiThreadObjMgr<ObjectType>::PRIME_NUM] =
{
    53l, 97l, 193l, 389l, 769l,
    1543l, 3079l, 6151l, 12289l, 24593l,
    49157l, 98317l, 196613l, 393241l, 786433l,
    1572869l, 3145739l, 6291469l, 12582917l, 25165843l,
    50331653l, 100663319l, 201326611l, 402653189l, 805306457l,
    1610612741l, 3221225473l, 4294967291l
};

}

#endif
