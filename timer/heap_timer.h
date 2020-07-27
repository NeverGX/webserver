#ifndef HEAP_TIMER
#define HEAP_TIMER

#include <time.h>

class Timer{
public:
    Timer(){}
    Timer(int delay, int fd, void (*func)(int)){
        sockfd = fd;
        cb_func=func;
        dead_time=time(NULL)+delay;
    }
    ~Timer(){}
    int sockfd;
    void (*cb_func)(int);
    time_t dead_time; 
};

class heap_timer{
public:
    heap_timer(int init_capacity):capacity(init_capacity),cur_size(0)
    {   
        assert(init_capacity>0);
        array = new Timer*[capacity];
        for(int i=0; i<capacity; i++){
            array[i]=NULL;
        }
    }
    ~heap_timer(){
        for (int i=0; i<cur_size; i++)
        {
            delete array[i];
        }
        delete []array;
    }
    void add_timer(Timer* timer){
        if(!timer) return;
        if(cur_size >= capacity){
            resize();
        }
        if(cur_size==0){
            array[cur_size++]=timer;
            return;
        }
        int currentnode=cur_size++;
        for(int p=(currentnode-1)/2;p>=0;p=(p-1)/2){
            if(array[p]->dead_time <= timer->dead_time){
                break;
            }
            else{
                //swap
                array[currentnode]=array[p];
                //当前节点向上移动
                currentnode=p;
            }
        }
        array[currentnode]=timer;
    }
    //优先级队列查找和删除的都是最大的元素或最小的元素
    void del_timer(Timer *timer){
        if(!timer){
            return;
        }
        //仅仅将目标定时器的回调函数设置为空，即延迟销毁，节省开销，但是这样容易使数组膨胀
        timer->cb_func=NULL;
    }
    Timer* top_timer(Timer *timer){
        if(empty()) return NULL;
        return array[0];
    }
    void pop_timer(){
        if(empty()) return;
        if(cur_size==1){
            delete array[0];
            cur_size--;
            array[0]=NULL;
        }
        else{
            delete array[0];
            array[0]=array[--cur_size];
            adjust_heap_timer(0);
        }
    }
    void tick(){
        Timer *tmp=array[0];
        time_t cur_time=time(NULL);
        while (!empty())
        {
            if(!tmp) break;
            if(tmp->dead_time > cur_time) break;
            if(tmp->cb_func){
                tmp->cb_func(tmp->sockfd);
            }
            pop_timer();
            tmp=array[0];
        }
    }
    void adjust_heap_timer(int p){
        int tmp = p;
        for(int child=p*2+1;child<=(cur_size-1);p*2+1)
        {
            if(child<(cur_size-1) && (array[child+1]->dead_time < array[child]->dead_time))
            {
                child++;
            }
            if(array[child]->dead_time < array[p]->dead_time){
                //swap
                Timer* tmp=array[p];
                array[p]=array[child];
                array[child]=tmp;
                //双亲节点向下移动
                p=child;
            }
            else
            {
                break;
            }
        }
    }
    void resize() 
    {
        Timer** temp = new Timer* [2*capacity];
        for( int i = 0; i < 2*capacity; ++i )
        {
            temp[i] = NULL;
        }
        capacity = 2*capacity;
        for ( int i = 0; i < cur_size; ++i )
        {
            temp[i] = array[i];
        }
        delete [] array;
        array = temp;
    }

    bool empty() const { return cur_size == 0; }
private:
    Timer** array;
    int capacity;
    int cur_size;//cur_size为最后一个元素的后一个位置

};

#endif