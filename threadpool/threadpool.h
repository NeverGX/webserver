//线程池类使用单例模式
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <assert.h>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <memory> 
#include "../lock/locker.h"

/*
类成员函数是被放在代码区,而类的静态成员变量在类定义时就已经在全局数据区分配了内存,因而它是属于类的,我们是使用这个类静态成员变量的一个拷贝.
对于非静态成员变量,我们是在类的实例化过程中(构造对象)才在栈区为其分配内存,是为每个对象生成一个拷贝,所以它是属于对象的. 
*/

/*
BUG：undefined reference to `threadpool<http_conn>::create_threadpool(int, int, int)'
解析：C++带有模板的类的代码只能写在头文件！！
模板类的实现代码，编译它的时候因为不知道套用什么参数，实际上没有任何有用的内容存在于.o文件当中。
而在使用模板类的地方指定了类型参数，编译器这才开始根据模板代码产生有用的.o编码，可是这些内容放在了使用模板的代码产生的.o文件当中。
如果使用模板代码的时候，通过include包含“看不到”模板的实现代码，这些所有的缺失，到链接阶段就无法完成。
*/

template <class T>
class threadpool{
private:
    threadpool(int thread_number,int max_requests, int mode);//构造函数私有
public:
   ~threadpool();//单例模式析构函数设为私有起到保护其不被delete,但是设为私有又不能被智能指针delete了
    //单例模式创建实例
    //这里是静态成员函数的原因，构造函数私有就无法创建对象实例，普通函数又只能通过对象名字调用，而静态成员函数能够通过类名直接调用
    static std::shared_ptr<threadpool<T>> create_threadpool(int thread_number,int max_requests, int mode);
    bool append(T *request);
    void run();
    void stop();
private:
    static void* worker(void* args);//worker必须是静态函数的原因，如果是普通成员函数就会有this指针，这样就有两个参数，pthread_create就会出错
    //智能指针解决内存泄漏，设为私有防止外界访问
    static std::shared_ptr<threadpool<T>> m_instance_ptr;//静态成员函数没有this指针，因此无法访问普通成员变量，因此这里实例只能是静态的
    int m_mode;//ator or proactor
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
std::shared_ptr<threadpool<T>> threadpool<T>::m_instance_ptr=nullptr;//静态成员变量要在外面定义，普通成员变量不能再类外定义，注意定义要前面加上数据类型

template <class T>
std::shared_ptr<threadpool<T>> threadpool<T>::create_threadpool(int thread_number,int max_requests, int mode)
    {
        if(m_instance_ptr==nullptr)
        {
            threadpool<T>* tmp= new threadpool<T>(thread_number,max_requests,mode);
            m_instance_ptr = std::shared_ptr<threadpool<T>>(tmp);
        }
        return m_instance_ptr;
    }

template <class T>
void* threadpool<T>::worker(void* args)
{
    /*
    Bug：这里为什么用threadpool<T>::m_instance_ptr->run()会产生Segmentation fault (core dumped)错误
    解析：此时正在执行构造函数，还没有返回赋值给静态成员变量m_instance_ptr（静态成员变量和函数为整个类共有）
    因此此时m_instance_ptr还是空指针，执行threadpool<T>::m_instance_ptr->run()命令时自然会发生段错误
    */
    ((threadpool<T>*)(args))->run();
    //threadpool<T>::m_instance_ptr->run();
    return NULL;
}

template <class T>
threadpool<T>::~threadpool()
{
    delete []m_threads;
}

template <class T>
threadpool<T>::threadpool(int thread_number,int max_requests, int mode):
m_thread_number(thread_number),m_max_requests(max_requests),m_mode(mode),m_threads(NULL),m_stop(false)
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

        //proactor模式
        if(0==m_mode)
        {
            request->process();
        }
        else
        {
            //reactor模式 
            if(0==request->m_state)//为读
            {
                if( request->read() )
                {
                    request->process();
                }
                else
                {
                    request->close_conn();
                    //if(request->m_timer)  timer_lst.del_timer(request->m_timer);
                }
            }
            else//否则为写
            {
                if( !request->write() )
                {
                    request->close_conn();
                    //if(request->m_timer)  timer_lst.del_timer(request->m_timer);
                }
            }
        }
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