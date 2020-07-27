//手写线程池单例模式
#include <unistd.h>
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <assert.h>
#include "../lock/locker.h"

template<class T>
class threadpool{
private:
    threadpool(int thread_number);
    static threadpool<T>* m_instance;
public:
    static threadpool<T>* create_threadpool(int thread_number){
        if(!m_instance){
            m_instance=new threadpool<T>(thread_number);
        }
        return m_instance;
    }
    void run();
private:
    static void* worker(void* args);
    int max_thread_number;
    locker m_lock;
};

template<class T>
threadpool<T>* threadpool<T>::m_instance=NULL;

template<class T>
threadpool<T>::threadpool(int thread_number):max_thread_number(thread_number){
    pthread_t m_threads[thread_number];
    for(int i=0;i<thread_number;i++){
        int ret =pthread_create(m_threads+i,NULL,worker,this);//bug出在这里，这里要用this，而不是m_instance,此时的m_instance还是空的
        assert(ret==0);
        ret = pthread_detach(m_threads[i]);
        assert(ret==0);
    }
}

template<class T>
void* threadpool<T>::worker(void* args){
    ((threadpool<T>*)args)->run();
    return NULL;
}

template<class T>
void threadpool<T>::run(){
    while(1){
        m_lock.lock();
        sleep(1);
        printf("a thread is running %d\n",max_thread_number);
        m_lock.unlock();
    }
}

int main(){
    threadpool<int>* pool = threadpool<int>::create_threadpool(6);
    sleep(2);
    delete pool;//pool被delete，子线程还是能够继续工作，delete一个指针时，指针指向的内存区域并不会被清空，但是此时这个内存区域可能会被分配
    /*原因
    该函数的调用地址在编译期间就已经确定，而且非虚函数的地址只跟类定义本身有关，与具体的实例对象无关，所以当对象的内存会销毁掉，
    并不会影响到对其非虚函数的调用。所以此次函数的调用可以成功。当然，如果该函数内部调用了对象的数据成员，还是会发生崩溃的。
    */
    printf("pool delete\n");
    sleep(6);
    return 0;
}