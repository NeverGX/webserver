#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <assert.h>
//循环数组实现工作队列
//循环数组，头部指针的后一个位置为第一个数据，尾部指针指向的位置为最后一个数据的位置
template <class T>
class work_queue
{
public:
    work_queue(int init_capacity=1024)
    {
        assert(init_capacity>=2);
        m_capacity=init_capacity;
        m_array= new T[m_capacity];
        m_size=0;
        m_front=m_capacity-1;
        m_back=m_capacity-1;
    }
    virtual ~work_queue()
    {
        delete []m_array;
    }
    bool empty();
    int size();
    T front();
    T back();
    void push(T task);
    void pop();
    void resize();
    
private:
    int m_size;
    T *m_array;
    int m_capacity;
    int m_front;
    int m_back;
    /*
    不需要锁，锁在日志类中添加
    pthread_mutex_t m_lock;//互斥锁
    pthread_cond_t m_cond;//条件变量
    */
};

template <class T>
bool work_queue<T>::empty(){
    return m_front==m_back;
}

template <class T>
int work_queue<T>::size(){
    return m_size;
}

template <class T>
T work_queue<T>::front(){
    return m_array[(m_front+1)%m_capacity];
}

template <class T>
T work_queue<T>::back(){
    return m_array[m_back%m_capacity];
}

template <class T>
void work_queue<T>::push(T task){
    if((m_back+1)%m_capacity==m_front)
    {
        resize();//动态扩容
    }
    m_back=(m_back+1)%m_capacity;
    m_array[m_back]=task;
    m_size++;
}

template <class T>
void work_queue<T>::pop(){
    if(empty())
    {
        return;
    }
    m_front=(m_front+1)%m_capacity;
    m_size--;
}

template <class T>
void work_queue<T>::resize(){
    T* temp = new T[2*m_capacity];
    for(int i=0;i<m_size;i++){
        temp[i]=m_array[(m_front+1+i)%m_capacity];
    } 
    m_capacity=2*m_capacity;
    m_front=m_capacity-1;
    m_back=m_size-1;
    delete []m_array;
    m_array=temp;
}

#endif