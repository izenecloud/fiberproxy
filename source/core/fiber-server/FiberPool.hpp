#ifndef FIBER_POOL_H_
#define FIBER_POOL_H_

#include <boost/fiber/all.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <deque>
#include <glog/logging.h>

namespace fibp
{

class FiberPool : public boost::enable_shared_from_this<FiberPool>
{
public:
    typedef boost::function<void()> fiber_task_t;
    FiberPool()
        :need_stop_(false), running_fiber_num_(0)
    {
    }
    ~FiberPool()
    {
        stop();
    }

    void init(std::size_t poolSize)
    {
        for(std::size_t i = 0; i < poolSize; ++i)
        {
            boost::shared_ptr<boost::fibers::fiber> tmp;
            tmp.reset(new boost::fibers::fiber(boost::bind(&FiberPool::run_fiber, shared_from_this())));
            fiber_pool_.push_back(tmp);
        }
        boost::shared_ptr<boost::fibers::fiber> tmp;
        tmp.reset(new boost::fibers::fiber(boost::bind(&FiberPool::check_task_fiber_fun,
                    shared_from_this())));
        fiber_pool_.push_back(tmp);
        LOG(INFO) << "fiber pool init done.";
    }

    // only the thread which already called init() is allowed to call run(), all the fibers in this pool will be scheduled in this thread.
    void run()
    {
        for(std::size_t i = 0; i < fiber_pool_.size(); ++i)
        {
            fiber_pool_[i]->join();
        }
        fiber_pool_.clear();
        LOG(INFO) << "fiber pool run exit.";
    }

    void stop()
    {
        {
            boost::unique_lock< boost::fibers::mutex > guard(task_lock_);
            need_stop_ = true;
        }
        cond_.notify_all();
    }

    void schedule_task(const fiber_task_t& task)
    {
        {
            boost::unique_lock< boost::mutex > guard_thread(task_thread_lock_);
            thread_task_list_.push_back(task);
        }
        thread_cond_.notify_one();
    }

    void check_task_fiber_fun()
    {
        boost::this_fiber::yield();
        while(true)
        {
            while(!fiber_task_list_.empty() && thread_task_list_.empty())
            {
                if (need_stop_)
                    return;
                boost::this_fiber::yield();
            }
            std::deque<fiber_task_t> tmp_list;
            {
                boost::unique_lock< boost::mutex > guard_thread(task_thread_lock_);
                while(thread_task_list_.empty())
                {
                    thread_cond_.timed_wait(guard_thread, boost::posix_time::milliseconds(100));
                    if (need_stop_)
                        return;
                    guard_thread.unlock();
                    boost::this_fiber::yield();
                    guard_thread.lock();
                }
                tmp_list.swap(thread_task_list_);
            }
            for (std::size_t i = 0; i < tmp_list.size(); ++i)
            {
                if (fiber_pool_.size() == 1)
                {
                    // no pre-created fibers, create a new fiber for each new task.
                    boost::fibers::fiber f(tmp_list[i]);
                    f.detach();
                }
                else
                {
                    fiber_task_list_.push_back(tmp_list[i]);
                    cond_.notify_one();
                }
                boost::this_fiber::yield();
            }
        }
    }

    // this interface can only be called from the fiber.
    void schedule_task_from_fiber(const fiber_task_t& task)
    {
        fiber_task_list_.push_back(task);
        // lazy init pool, only init pool when the task is pushed from the main fiber,
        // and resize the pool as needed while the task is growing.
        if (fiber_pool_.empty())
        {
            fiber_pool_.reserve(GROW_SIZE);
            for(std::size_t i = 0; i < GROW_SIZE; ++i)
            {
                boost::shared_ptr<boost::fibers::fiber> tmp;
                tmp.reset(new boost::fibers::fiber(
                        boost::bind(&FiberPool::run_fiber, shared_from_this())));
                fiber_pool_.push_back(tmp);
            }
            LOG(INFO) << "growing fiber pool to : " << fiber_pool_.size()
                << ", running: " << running_fiber_num_;
        }
        else if (running_fiber_num_ >= fiber_pool_.size() - 1)
        {
            // grow the fiber number since the waiting task is growing.
            std::size_t grow_size = fiber_pool_.size();
            if (grow_size > MAX_GROW_SIZE)
                grow_size = 0;
            fiber_pool_.reserve(fiber_pool_.size() + grow_size);
            for(std::size_t i = 0; i < grow_size; ++i)
            {
                boost::shared_ptr<boost::fibers::fiber> tmp;
                tmp.reset(new boost::fibers::fiber(
                        boost::bind(&FiberPool::run_fiber, shared_from_this())));
                fiber_pool_.push_back(tmp);
            }
            LOG(INFO) << "growing fiber pool to : " << fiber_pool_.size()
                << ", running: " << running_fiber_num_;
        }
        cond_.notify_one();
        boost::this_fiber::yield();
    }

private:
    void run_fiber()
    {
        while(true)
        {
            try
            {
                fiber_task_t task;
                {
                    boost::unique_lock< boost::fibers::mutex > guard(task_lock_);

                    if (need_stop_)
                        return;
                    while (fiber_task_list_.empty())
                    {
                        cond_.wait(guard);
                        if (need_stop_)
                        {
                            return;
                        }
                    }
                    task = fiber_task_list_.front();
                    fiber_task_list_.pop_front();
                }
                if (task)
                {
                    ++running_fiber_num_;
                    task();
                    --running_fiber_num_;
                }
            }
            catch(const boost::fibers::fiber_interrupted& e)
            {
                break;
            }
        }
    }
    std::vector<boost::shared_ptr<boost::fibers::fiber> > fiber_pool_;
    std::deque<fiber_task_t>  fiber_task_list_;
    std::deque<fiber_task_t>  thread_task_list_;
    boost::fibers::condition_variable  cond_;
    boost::fibers::mutex task_lock_;
    boost::mutex task_thread_lock_;
    boost::condition_variable  thread_cond_;
    bool need_stop_;
    std::size_t running_fiber_num_;
    static const std::size_t GROW_SIZE = 10;
    static const std::size_t MAX_GROW_SIZE = 15000;
};

typedef boost::shared_ptr<FiberPool> fiber_pool_ptr_t;

class FiberPoolMgr
{
public:
    typedef boost::unordered_map<boost::thread::id, boost::shared_ptr<FiberPool> >  PoolContainerT;
    FiberPoolMgr()
    {
        fiber_pool_list_.rehash(1000);
    }

    void initFiberPool(const boost::thread::id& tid)
    {
        fiber_pool_list_[tid].reset(new FiberPool());
    }

    fiber_pool_ptr_t getFiberPool()
    {
        static uint32_t index = 0;
        PoolContainerT::iterator it = fiber_pool_list_.find(boost::this_thread::get_id());
        if (it == fiber_pool_list_.end())
        {
            int round = index % fiber_pool_list_.size();
            it = fiber_pool_list_.begin();
            while(it != fiber_pool_list_.end())
            {
                if (--round <= 0)
                {
                    break;
                }
                ++it;
            }
            if (it == fiber_pool_list_.end())
            {
                it = fiber_pool_list_.begin();
            }
        }

        return it->second;
    }

    void runFiberPool(const boost::thread::id& tid)
    {
        PoolContainerT::iterator it = fiber_pool_list_.find(tid);
        if(it != fiber_pool_list_.end())
        {
            LOG(INFO) << "init fiber pool for thread.";
            it->second->init(FIBER_NUM_FOR_THREAD);
            LOG(INFO) << "run fiber pool for thread.";
            it->second->run();
        }
    }

    void stopFiberPool()
    {
        PoolContainerT::iterator it = fiber_pool_list_.begin();
        while(it != fiber_pool_list_.end())
        {
            it->second->stop();
            ++it;
        }
    }

private:
    PoolContainerT  fiber_pool_list_;
    static const int FIBER_NUM_FOR_THREAD = 500;
};


}

#endif
