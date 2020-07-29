#ifndef LOG_H
#define LOG_H

#include <string>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <fstream> 
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "work_queue.h"
#include <pthread.h>
#include <stdarg.h>
#include <assert.h>

//单例模式日志类
class Log
{
private:
    Log();
    virtual ~Log();//单例模式析构函数设为私有起到保护其不被delete
public:
    static Log* get_instance();//获取唯一实例
    static void* worker(void* args);
    bool init(int close_log,int log_buf_size= 256,int split_lines = 5000000);
    void run();
    void write_log(int level, const char *format, ...);
    int m_close_log; //关闭日志
private:
    int m_split_lines;  //日志最大行数
    long long m_count;  //日志行数记录
    int m_log_buf_size;  //缓冲区大小
    int m_today;        //因为按天分类,记录当前时间是那一天
    FILE *m_fp;         //打开log的文件指针
    work_queue<char*> m_log_queue; //工作队列
    pthread_mutex_t m_mutex;//互斥锁
    pthread_cond_t m_cond;//条件变量
    pthread_t m_pthread;
};

#define LOG_DEBUG(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__);}
#define LOG_WARN(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__);}
#define LOG_ERROR(format, ...) if(0 ==Log::get_instance()-> m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__);}

#endif