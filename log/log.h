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
    virtual ~Log();
public:
    static Log* get_instance();//获取唯一实例
    static void* worker(void* args);
    bool init(int close_log=false, int log_buf_size= 256,int split_lines = 5000000);
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

//c++11标准获取局部静态实例无需加锁
Log* Log::get_instance()
{
    static Log m_log;
    return &m_log;
}

Log::Log()
{
    //方式一初始化互斥锁和条件变量
    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond,NULL);
    
    //方式二初始化互斥锁和条件变量
    /*
    m_mutex=PTHREAD_MUTEX_INITIALIZER;
    m_cond=PTHREAD_COND_INITIALIZER;
    */

     //创建日志线程
    int ret = pthread_create(&m_pthread,NULL,worker,NULL);
    assert(ret==0);
    //分离线程
    ret = pthread_detach(m_pthread);
    assert(ret==0);

}

Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}

void* Log::worker(void* args)
{
    Log::get_instance()->run();
    return NULL;
}

void Log::run(){
    while (true)
    {
        pthread_mutex_lock(&m_mutex);
        while(m_log_queue.empty())
        {
            assert(pthread_cond_wait(&m_cond,&m_mutex)==0);
        }
        char* msg = m_log_queue.front();
        m_log_queue.pop();
        fputs(msg, m_fp);
        fflush(m_fp);//fflush()会强迫将缓冲区内的数据写回参数m_fp指定的文件中
        pthread_mutex_unlock(&m_mutex);
        delete msg;
        msg=NULL;
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(int close_log, int log_buf_size, int split_lines)
{
    m_count = 0;
    m_close_log = close_log;
    m_log_buf_size= log_buf_size;
    m_split_lines = split_lines;
    
    time_t t = time(NULL);
    struct tm *my_tm = localtime(&t);
    /*
    struct tm 
    {
        int tm_sec;         // 秒，范围从 0 到 59         
        int tm_min;         // 分，范围从 0 到 59                
        int tm_hour;        // 小时，范围从 0 到 23                
        int tm_mday;        // 一月中的第几天，范围从 1 到 31                    
        int tm_mon;         // 月份，范围从 0 到 11                
        int tm_year;        // 自 1900 起的年数                
        int tm_wday;        // 一周中的第几天，范围从 0 到 6                
        int tm_yday;        // 一年中的第几天，范围从 0 到 365                    
        int tm_isdst;       // 夏令时                         
    };
    */
    m_today = my_tm->tm_mday;

    char log_dir[256] = {0};
    char log_file_name[32]={0};
    strcpy(log_dir,"/home/wangkh/cpp_ex/Mywebserver/log/");
    snprintf(log_file_name, 32, "%d_%02d_%02d_%lld",my_tm->tm_year + 1900, my_tm->tm_mon + 1, my_tm->tm_mday, m_count/m_split_lines);
    strcat(log_dir,log_file_name);
    m_fp = fopen(log_dir, "a");
    if (m_fp == NULL)
    {
        return false;
    }
    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    /*
    struct timeval
    {
        long int tv_sec; // 秒数
        long int tv_usec; // 微秒数
    }
    */
    struct timeval now = {0, 0};
    //gettimeofday获得当前精确时间
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *my_tm = localtime(&t);
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    //写入一个log，对m_count++, m_split_lines最大行数
    pthread_mutex_lock(&m_mutex);
    m_count++;
    if (m_today != my_tm->tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        fflush(m_fp);
        fclose(m_fp);
        char new_log_dir[256] = {0};
        char new_log_file_name[32] = {0};
        if (m_today != my_tm->tm_mday)
        {
            m_today = my_tm->tm_mday;
            m_count = 0;
        }
        strcpy(new_log_dir,"/home/wangkh/cpp_ex/Mywebserver/log/");
        snprintf(new_log_file_name, 32, "%d_%02d_%02d_%lld", my_tm->tm_year + 1900, my_tm->tm_mon + 1, my_tm->tm_mday,m_count/m_split_lines);
        strcat(new_log_dir,new_log_file_name);
   
        m_fp = fopen(new_log_dir, "a");
    }
    pthread_mutex_unlock(&m_mutex);

    //写入的具体时间内容格式
    va_list valst;
    va_start(valst, format);
    char *m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm->tm_year + 1900, my_tm->tm_mon + 1, my_tm->tm_mday,
                     my_tm->tm_hour, my_tm->tm_min,my_tm->tm_sec,now.tv_usec,s);
    
    int m = vsnprintf(m_buf + n, m_log_buf_size-n, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    va_end(valst);

    //异步写入
    pthread_mutex_lock(&m_mutex);
    m_log_queue.push(m_buf);
    pthread_mutex_unlock(&m_mutex);
    pthread_cond_signal(&m_cond);

}

#define LOG_DEBUG(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__);}
#define LOG_WARN(format, ...) if(0 == Log::get_instance()->m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__);}
#define LOG_ERROR(format, ...) if(0 ==Log::get_instance()-> m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__);}

#endif