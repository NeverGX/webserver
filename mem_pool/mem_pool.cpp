#include <iostream>
#include <stdlib.h>
//#include <new>

class allocator{
private:
    struct obj
    {
        obj* next;
    };
public:
    void* allocate(size_t size);
    void deallocate(void* p,size_t size);
private:
    obj* freestore=NULL;
    const int chunk=5;
};

void* allocator::allocate(size_t size)
{
    obj* p;
    if(!freestore)
    {
        size_t CHUNK= size*chunk;
        freestore=p=(obj*)malloc(CHUNK);
        for(int i=0;i<chunk-1;i++)
        {
            p->next=(obj*)((char*)p+size);
            p=p->next;
        }
        p->next=NULL;
    }
    p=freestore;
    freestore=freestore->next;
    return p;
}

void allocator::deallocate(void* p,size_t size)
{
    ((obj*)p)->next=freestore;
    freestore=(obj*)p;
}


class Foo{
public:
    long data;
    /*bug解析
    64位系统指针的大小为8字节
    所以数据最小也要8字节
    所以如果是int就会报错
    */

    Foo(int x):data(x){}
    static allocator my_allocator;
    void* operator new(size_t size)
    {
        return my_allocator.allocate(size);
    }
    void operator delete(void* p,size_t size)
    {
        return my_allocator.deallocate(p,size);
    }
};
allocator Foo::my_allocator;

int main()
{
    Foo* p[100];
    std::cout<<"sizeof FOO: "<<sizeof(Foo)<<std::endl;
    for(int i=0;i<23;i++)
    {
        p[i]=new Foo(i);
        std::cout<<p[i]<<' '<<p[i]->data<<std::endl;
    }
    for(int i=0;i<23;i++)
    {
        delete p[i];
    }
}