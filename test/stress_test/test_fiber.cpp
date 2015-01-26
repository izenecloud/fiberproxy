
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE testfiber
#include <sstream>
#include <string>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/utility.hpp>

#include <boost/fiber/all.hpp>
#include <glog/logging.h>

void f1() {}

void f2()
{
    boost::this_fiber::yield();
}

void f4()
{
    boost::fibers::fiber s( f2);
    BOOST_CHECK( s);
    BOOST_CHECK( s.joinable() );
    std::cerr << "fiber start at f4: " << s.get_id() << std::endl;
    s.join();
    BOOST_CHECK( ! s);
    BOOST_CHECK( ! s.joinable() );
    std::cerr << "fiber exit at f4: " << s.get_id() << std::endl;
}

void f6( int & i)
{
    i = 1;
    boost::this_fiber::yield();
    i = 1;
    boost::this_fiber::yield();
    i = 2;
    boost::this_fiber::yield();
    i = 3;
    boost::this_fiber::yield();
    i = 5;
    boost::this_fiber::yield();
    i = 8;
}

void f7( int & i, bool & failed)
{
    try
    {
        i = 1;
        boost::this_fiber::yield();
        boost::this_fiber::interruption_point();
        i = 1;
        boost::this_fiber::yield();
        boost::this_fiber::interruption_point();
        i = 2;
        boost::this_fiber::yield();
        boost::this_fiber::interruption_point();
        i = 3;
        boost::this_fiber::yield();
        boost::this_fiber::interruption_point();
        i = 5;
        boost::this_fiber::yield();
        boost::this_fiber::interruption_point();
        i = 8;
    }
    catch ( boost::fibers::fiber_interrupted const&)
    { failed = true; }
}

void interruption_point_wait(boost::fibers::mutex* m,bool* failed)
{
    boost::unique_lock<boost::fibers::mutex> lk(*m);
    boost::this_fiber::interruption_point();
    *failed=true;
}

void disabled_interruption_point_wait(boost::fibers::mutex* m,bool* failed)
{
    boost::unique_lock<boost::fibers::mutex> lk(*m);
    boost::this_fiber::disable_interruption dc;
    boost::this_fiber::interruption_point();
    *failed=false;
}

void interruption_point_join( boost::fibers::fiber & f)
{
    f.join();
}

void test_id()
{
    boost::fibers::fiber s1;
    boost::fibers::fiber s2( f2);
    BOOST_CHECK( ! s1);
    BOOST_CHECK( s2);

    BOOST_CHECK_EQUAL( boost::fibers::fiber::id(), s1.get_id() );
    BOOST_CHECK( boost::fibers::fiber::id() != s2.get_id() );

    boost::fibers::fiber s3( f1);
    BOOST_CHECK( s2.get_id() != s3.get_id() );

    s1 = boost::move( s2);
    BOOST_CHECK( s1);
    BOOST_CHECK( ! s2);

    BOOST_CHECK( boost::fibers::fiber::id() != s1.get_id() );
    BOOST_CHECK_EQUAL( boost::fibers::fiber::id(), s2.get_id() );

    BOOST_CHECK( ! s2.joinable() );

    s1.join();
    s3.join();
}

void test_detach()
{
    {
        boost::fibers::fiber s1( f1);
        BOOST_CHECK( s1);
        s1.detach();
        BOOST_CHECK( ! s1);
        BOOST_CHECK( ! s1.joinable() );
    }

    {
        boost::fibers::fiber s2( f2);
        BOOST_CHECK( s2);
        s2.detach();
        BOOST_CHECK( ! s2);
        BOOST_CHECK( ! s2.joinable() );
    }
}

void test_replace()
{
    boost::fibers::round_robin ds;
    boost::fibers::set_scheduling_algorithm( & ds);
    boost::fibers::fiber s1( f1);
    BOOST_CHECK( s1);
    boost::fibers::fiber s2( f2);
    BOOST_CHECK( s2);

    s1.join();
    s2.join();
}

void test_complete()
{
    boost::fibers::fiber s1( f1);
    BOOST_CHECK( s1);
    boost::fibers::fiber s2( f2);
    BOOST_CHECK( s2);

    s1.join();
    s2.join();
}

void test_join_in_thread()
{
    boost::fibers::fiber s( f2);
    BOOST_CHECK( s);
    BOOST_CHECK( s.joinable() );
    s.join();
    BOOST_CHECK( ! s);
    BOOST_CHECK( ! s.joinable() );
}

void test_join_and_run()
{
    boost::fibers::fiber s( f2);
    BOOST_CHECK( s);
    BOOST_CHECK( s.joinable() );
    s.join();
    BOOST_CHECK( ! s.joinable() );
    BOOST_CHECK( ! s);
}

void test_join_in_fiber()
{
    // spawn fiber s
    // s spawns an new fiber s' in its fiber-fn
    // s' yields in its fiber-fn
    // s joins s' and gets suspended (waiting on s')
    boost::fibers::fiber s( f4);
    // run() resumes s + s' which completes
    std::cerr << "fiber start at test_join_in_fiber: " << s.get_id() << std::endl;
    s.join();
    //BOOST_CHECK( ! s);
}

void test_yield()
{
    int v1 = 0, v2 = 0;
    BOOST_CHECK_EQUAL( 0, v1);
    BOOST_CHECK_EQUAL( 0, v2);
    boost::fibers::fiber s1( boost::bind( f6, boost::ref( v1) ) );
    boost::fibers::fiber s2( boost::bind( f6, boost::ref( v2) ) );
    std::cerr << "fiber start at test_yield: " << s1.get_id() << s2.get_id() << std::endl;
    s1.join();
    s2.join();
    BOOST_CHECK( ! s1);
    BOOST_CHECK( ! s2);
    BOOST_CHECK_EQUAL( 8, v1);
    BOOST_CHECK_EQUAL( 8, v2);
}

void test_fiber_interrupts_at_interruption_point()
{
    boost::fibers::mutex m;
    bool failed=false;
    bool interrupted = false;
    boost::unique_lock<boost::fibers::mutex> lk(m);
    boost::fibers::fiber f(boost::bind(&interruption_point_wait,&m,&failed));
    std::cerr << "fiber start at test_fiber_interrupts_at_interruption_point: " << f.get_id() << std::endl;
    f.interrupt();
    lk.unlock();
    try
    { f.join(); }
    catch ( boost::fibers::fiber_interrupted const& e)
    { interrupted = true; }
    BOOST_CHECK( interrupted);
    BOOST_CHECK(!failed);
}

void test_fiber_no_interrupt_if_interrupts_disabled_at_interruption_point()
{
    boost::fibers::mutex m;
    bool failed=true;
    boost::unique_lock<boost::fibers::mutex> lk(m);
    boost::fibers::fiber f(boost::bind(&disabled_interruption_point_wait,&m,&failed));
    std::cerr << "fiber start at test_fiber_no_interrupt_if_interrupts_disabled_at_interruption_point : " << f.get_id() << std::endl;
    f.interrupt();
    lk.unlock();
    f.join();
    BOOST_CHECK(!failed);
}

void test_fiber_interrupts_at_join()
{
    int i = 0;
    bool failed = false;
    boost::fibers::fiber f1( boost::bind( f7, boost::ref( i), boost::ref( failed) ) );
    boost::fibers::fiber f2( boost::bind( interruption_point_join, boost::ref( f1) ) );
    std::cerr << "fiber start : " << f1.get_id() << ", " << f2.get_id() << std::endl;
    f1.interrupt();
    f2.join();
    BOOST_CHECK_EQUAL( 1, i);
    BOOST_CHECK( failed);
    BOOST_CHECK_EQUAL( 1, i);
}

//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <sstream>
#include <string>

#include <boost/assert.hpp>
#include <vector>
#include <boost/bind.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/cstdint.hpp>
#include <boost/function.hpp>
#include <boost/ref.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/utility.hpp>

#include <boost/fiber/all.hpp>

typedef boost::chrono::nanoseconds  ns;
typedef boost::chrono::milliseconds ms;


void f_wait(boost::fibers::mutex* m, boost::fibers::condition* c, int timeout, boost::fibers::barrier& b)
{
    boost::unique_lock< boost::fibers::mutex > lk( *m);
    //std::cerr << "fiber wait begin: " << boost::this_fiber::get_id() << std::endl;
    boost::chrono::system_clock::time_point t0 = boost::chrono::system_clock::now();
    boost::chrono::system_clock::time_point t = t0 + ms(timeout * 1000);
    b.wait();
    (*c).wait_until(lk, t);
}

void test_fiber_scheduler()
{

    int waiting_num = 9;
    int ready_num = 3;
    boost::fibers::barrier b( waiting_num + ready_num + 1);
    std::vector<boost::fibers::fiber*> waiting_fiber_list;
    std::vector<boost::fibers::mutex*> waiting_mutex_list;
    std::vector<boost::fibers::condition*> waiting_condition_list;
    waiting_fiber_list.reserve(waiting_num);
    waiting_mutex_list.reserve(waiting_num);
    waiting_condition_list.reserve(waiting_num);
    for(int i = 0; i < waiting_num; ++i)
    {
        boost::fibers::mutex* m = new boost::fibers::mutex;
        boost::fibers::condition* c = new boost::fibers::condition;
        boost::fibers::fiber* t = new boost::fibers::fiber(boost::bind(&f_wait, m, c, 1, boost::ref(b)));
        waiting_fiber_list.push_back(t);
        waiting_mutex_list.push_back(m);
        waiting_condition_list.push_back(c);
    }


    std::vector<boost::fibers::fiber*> ready_fiber_list;
    std::vector<boost::fibers::mutex*> ready_mutex_list;
    std::vector<boost::fibers::condition*> ready_condition_list;
    ready_fiber_list.reserve(ready_num);
    ready_mutex_list.reserve(ready_num);
    ready_condition_list.reserve(ready_num);
    for(int i = 0; i < ready_num; ++i)
    {
        boost::fibers::mutex* m = new boost::fibers::mutex;
        boost::fibers::condition* c = new boost::fibers::condition;
        boost::fibers::fiber* t = new boost::fibers::fiber(boost::bind(&f_wait, m, c, 2, boost::ref(b)));
        ready_fiber_list.push_back(t);
        ready_mutex_list.push_back(m);
        ready_condition_list.push_back(c);
    }
    b.wait();
    boost::chrono::system_clock::time_point t0 = boost::chrono::system_clock::now();
    for(int i = 1; i < ready_num; ++i)
    {
        ready_mutex_list[i]->lock();
        //std::cerr << "begin notify fiber: " << ready_fiber_list[i]->get_id() << std::endl;
        ready_condition_list[i]->notify_one();
        ready_mutex_list[i]->unlock();
        //std::cerr << "begin join fiber: " << ready_fiber_list[i]->get_id() << std::endl;
        ready_fiber_list[i]->join();
        //std::cerr << "join finished fiber: " << ready_fiber_list[i]->get_id() << std::endl;
        delete ready_mutex_list[i];
        delete ready_condition_list[i];
        delete ready_fiber_list[i];
    }
    sleep(2);
    for(int i = 0; i < waiting_num; ++i)
    {
        waiting_mutex_list[i]->lock();
        waiting_condition_list[i]->notify_one();
        waiting_mutex_list[i]->unlock();
        waiting_fiber_list[i]->join();
        delete waiting_mutex_list[i];
        delete waiting_condition_list[i];
        delete waiting_fiber_list[i];
    }
    boost::chrono::system_clock::time_point t1 = boost::chrono::system_clock::now();
    std::cerr << "cost time: " << boost::chrono::duration_cast<boost::chrono::milliseconds>(t1 - t0).count() << std::endl;
    ready_condition_list[0]->notify_one();
    ready_fiber_list[0]->join();
}

BOOST_AUTO_TEST_SUITE(TestFiberSuite)

BOOST_AUTO_TEST_CASE(testfiber)
{
    //test_id();
    //test_detach();
    //test_complete();
    //test_join_in_thread();
    //test_join_and_run();
    //test_join_in_fiber();
    //test_yield();
    //test_fiber_interrupts_at_interruption_point();
    //test_fiber_no_interrupt_if_interrupts_disabled_at_interruption_point();
    //test_fiber_interrupts_at_join();
    //test_replace();
}

BOOST_AUTO_TEST_CASE(testmutex)
{
    //test_mutex();
    //test_recursive_mutex();
    //test_recursive_timed_mutex();
    //test_timed_mutex();
}

BOOST_AUTO_TEST_CASE(testcondition)
{
    //test_one_waiter_notify_one();
    //test_two_waiter_notify_one();
    //test_two_waiter_notify_all();
    //test_condition_wait();
    //test_condition_wait_is_a_interruption_point();
    //test_condition_wait_until();
    //test_condition_wait_until_pred();
    //test_condition_wait_for();
    //test_condition_wait_for_pred();

    test_fiber_scheduler();
}

BOOST_AUTO_TEST_SUITE_END()

