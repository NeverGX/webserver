#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "../utils/utils.h"
#include "../timer/list_timer.h"

class http_conn{
public:
    static const int BUFFER_SIZE=1024;
    http_conn():m_sockfd(-1){}
    ~http_conn(){}
    void init( int sockfd, sockaddr_in addr, util_timer *timer );
    void close_conn();
    void process();
    bool read();
    bool write(int remote_sockfd);
    static int m_epollfd;//静态成员变量内部声明，注意只是声明
    static int m_user_count;
    int m_sockfd;
    sockaddr_in m_address;
    char buf[BUFFER_SIZE];
    util_timer* m_timer;
};
int http_conn::m_epollfd=-1;//静态变量一定要在外面定义，注意时定义不是赋值
int http_conn::m_user_count=0;//静态变量一定要在外面定义，注意时定义不是赋值
void http_conn::init(int sockfd, sockaddr_in addr, util_timer *timer){
    m_sockfd=sockfd;
    m_address=addr;
    m_timer=timer;
    addfd( m_epollfd, sockfd,true,true);
    m_user_count++;
}
bool http_conn::read(){
    memset(buf,0,sizeof(buf));
    while (true)
    {
        int ret = recv(m_sockfd,buf,BUFFER_SIZE-1,0);
        if(ret==-1){
            if(errno==EAGAIN) break;
            return false;
        }
        else if(ret==0){
            return false;
        }
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT);//通知写事件
    return true;
}

bool http_conn::write(int remote_sockfd){
    int write_bias=0;
    int msg_len=strlen(buf);
    while (true)
    {
        int ret = send(remote_sockfd,buf+write_bias,strlen(buf)-write_bias,0);
        if(ret==-1){
            if(errno==EAGAIN) continue;
            return false;
        }
        else if((ret+write_bias)==msg_len){
            break;
        }
        else if(0<ret<msg_len){
            write_bias+=ret;
        }
    }
    return true;
}

void http_conn::close_conn(){
    removefd( m_epollfd, m_sockfd );
    //printf( "close fd %d\n", m_sockfd );
    m_sockfd = -1;
    m_user_count--;
}

void http_conn::process(){
    printf("Process Message-----------------\n");
    sleep(2);
    modfd(m_epollfd,m_sockfd,EPOLLOUT);//通知可写事件
}

#endif