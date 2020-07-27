//线程池类使用单例模式
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <assert.h>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"

//如果主线程先结束，然后main函数结束，调用了exit()结束了进程，那么所有子线程也都退出了
template <class T>
class threadpool{
private:
    threadpool(int thread_number,int max_requests);
public:
    ~threadpool(){delete []m_threads;}//析构函数一定不能私有，不然delete操作符无法访问
    //单例模式创建实例
    //这里是静态成员函数的原因，构造函数私有就无法创建对象实例，普通函数又只能通过对象名字调用，而静态成员函数能够通过类名直接调用
    static threadpool<T>* create_threadpool(int thread_number,int max_requests)
    {
      
        if(!m_instance)
        {
        m_instance=new threadpool<T>(thread_number,max_requests);
        }
        return m_instance;
    }

    bool append(T *request);
    void run();
    void stop();

private:

    static void* worker(void* args);//worker必须是静态函数的原因，如果是普通成员函数就会有this指针，这样就有两个参数，pthread_create就会出错

    //私有静态指针变量指向唯一实例
    static threadpool<T>* m_instance;//静态成员函数没有this指针，因此无法访问普通成员变量，因此这里实例只能是静态的

    int m_thread_number;//线程池的线程数
    int m_max_requests;//请求队列中允许的最大请求数
    pthread_t* m_threads;//描述线程池的数组，线程id
    std::list<T*> m_request_queue;//请求队列
    locker m_locker;//互斥锁,默认初始化
    cond m_cond;//条件变量，默认初始化
    sem m_sem;
    bool m_stop;
};

template <class T>//私有静态变量也无法在外面进行访问，不过可以在外面进行定义
threadpool<T>* threadpool<T>::m_instance=NULL;//静态成员变量要在外面定义，普通成员变量不能再类外定义，注意定义要前面加上数据类型

template <class T>
threadpool<T>::threadpool(int thread_number,int max_requests):
m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL),m_stop(false)
{
    if( (thread_number<=0) || (max_requests<=0) ) throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) throw std::exception();
    for(int i=0;i<m_thread_number;i++)
    {
        //创建线程
        //bug出在这里，这里要用this，而不是m_instance!!!!此时的m_instance还是空的,用m_instance就会导致无法调用m_lock
        int ret = pthread_create(m_threads+i,NULL,worker,this);
        assert(ret==0);
        //线程分离就是当线程被设置为分离状态后，线程结束时，它的资源会被系统自动的回收，而不再需要在其它线程中对其进行pthread_join()操作。
        //线程默认是joinable的
        ret = pthread_detach(m_threads[i]);
        assert(ret==0);
    }   
}

//往请求队列里插入请求
template <class T>
bool threadpool<T>::append(T* request)
{
    m_locker.lock();//加锁
    if(m_request_queue.size()>m_max_requests)
    {
        m_locker.unlock();
        return false;
    }
    m_request_queue.push_back(request);
    m_locker.unlock();
    m_cond.signal();//条件变量唤醒睡眠队列
    //m_sem.post();////信号量方式

    return true;
}

template <class T>
void* threadpool<T>::worker(void* args)
{
    ((threadpool<T>*)args)->run();
    return NULL;
}

template <class T>
void threadpool<T>::run()
{
    while (!m_stop)
    {   
        //m_sem.wait();//信号量方式
        m_locker.lock();
        
        //只有工作队列为空时才等待，减少锁的消耗
        while(m_request_queue.empty() && !m_stop)//若是wait前工作队列就有任务了，那么生产者解锁后就不会进入这个循环，也不存在唤醒丢失
        {
            //条件变量函数成功时返回0
            m_cond.wait(m_locker.get());//调用m_cond.wait前加锁，防进入循环后，还没把线程放入条件变量等待的这段时间，不会被唤醒，防止唤醒丢失
        }
        
        T* request = m_request_queue.front();
        if(!m_request_queue.empty())
        {
            //这里的bug在于唤醒后，一个线程被销毁，但是没解锁，其他进程还是被锁住，导致只能结束这一个进程
           m_request_queue.pop_front();//通过m_stop结束上面的循环时，队列可能为空，pop_front就会报错，因此一定要在上面加个判断条件 
        } 
        m_locker.unlock();
        if(!request) continue;
        request->process();
        
    }
}

template <class T>
void threadpool<T>::stop()
{
    m_locker.lock();
    m_stop=true;
    m_locker.unlock();
    m_cond.broadcast();
}

#endif