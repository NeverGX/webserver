#include "./http_conn/http_conn.h"
#include "./threadpool/threadpool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5
#define TIME_CAPCITY 65536
static heap_timer timer_lst(1024);
static int epollfd;
static int sigpipefd[2];//处理信号事件的管道
static int mode=0;//0为proactor，1为reactor

/*
用GDB检测段错误(Segmentation fault (core dumped))发生的位置
1.执行ulimit -c unlimited语句后再运行a.out就会生成core文件
2.用gdb来调试core文件：gdb a.out core
3.在gdb字符界面中敲“where”查看更详细信息。
*/

/*
全局区创造实例bug分析
例1：
Log* log=Log::get_instance();
log->init()//error: expected constructor, destructor, or type conversion
解析：C++程序的基本单元是函数，也就是说语句都要写在函数体内。一般来说，除了预编译，结构体、类的声明外，
只有对全局变量或静态变量赋初值的语句可以写在函数体外。即只有在花括号之内才可以执行程序语句!!!
*/

void addsig( int sig, sighandler_t handler, bool restart = true )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );//只有信号处理函数运行时才屏蔽所有信号
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void sig_handler( int sig )//统一事件源信号处理函数
{
    int save_errno = errno;
    int msg = sig;
    send(sigpipefd[1],(char*)&msg, 1, 0 );
    errno = save_errno;
}

void timer_handler()//定时器处理函数
{
    timer_lst.tick();
    alarm( TIMESLOT );
}

void cb_func( int sockfd )//定时器回调函数
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, sockfd, 0 );
    close( sockfd );
    http_conn::m_user_count--;
    LOG_INFO( "connection time out close fd %d", sockfd );
}

int main(int argc, char* argv[]){
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    //初始化log日志,true为关闭日志
    Log::get_instance()->init(true); //为什么只在main内有效？
   
    //设置信号掩码，在主线创建其他子线程之前调用pthread_sigmask设置好信号掩码，所有新创建的线程都会继承这个掩码
    sigset_t mask_set;
    sigset_t old_mask_set;
    sigfillset(&mask_set);
    pthread_sigmask(SIG_SETMASK,&mask_set,&old_mask_set);//屏蔽所有子线程的信号，保存原来的信号掩码
    //创建线程池
    std::shared_ptr<threadpool<http_conn>> pool= threadpool<http_conn>::create_threadpool(8,10000,mode);
    //创建线程池之后在主线程恢复原来的信号掩码，用主线程处理所有信号
    pthread_sigmask(SIG_SETMASK,&old_mask_set,NULL);//SIG_SETMASK直接将信号掩码设置为_set,SIG_BLOCK时当前掩码和_set并集，

    //添加感兴趣的信号
    addsig(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
    addsig(SIGALRM,sig_handler);//捕获定时器信号
    //addsig(SIGINT,sig_handler);//捕获ctrl+c信号

    //创建用户
    http_conn* users=new http_conn[MAX_FD];

    //socket创建常规流程
    //Linux内核2.6.17后，type参数可以接受下面两个相与SOCK_NONBLOCK和SOCK_CLOEXEC，分别表示设置为非阻塞，以及fork时在子进程中关闭该socket
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    int ret = 0;
    struct sockaddr_in address;
    memset(&address,0,sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int reuse = 1;//设置timewait不等待
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0 );

    ret = listen( listenfd, 5 );
    assert( ret >= 0 );

    //IO多路复用之epoll
    epoll_event events[ MAX_EVENT_NUMBER ];
    epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd_LT( epollfd, listenfd, false, true);//采用非阻塞模式和LT模式
    /*为什么要将listenfd设置为非阻塞
    客户通过connect向TCP服务器发起三次握手
    三次握手完成后，触发TCP服务器监听套接字的可读事件，IO复用返回（select、poll、epoll_wait）
    客户通过RST报文取消连接
    TCP服务器调用accept接受连接，此时发现内核已连接队列为空（因为唯一的连接已被客户端取消）
    程序阻塞在accept调用，无法响应其它已连接套接字的事件
    */

    //设置用户的epollfd
    http_conn::m_epollfd = epollfd;

    //创建处理信号的事件的双向管道
    ret = socketpair(PF_UNIX,SOCK_STREAM,0,sigpipefd);
    assert(ret!=-1);
    setnonblocking( sigpipefd[1] );
    addfd( epollfd, sigpipefd[0],false,true);

    //定时
    alarm(TIMESLOT);
    bool timeout=false;

    bool stop=false; 

    while( !stop )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        /*
        如果程序在执行处于阻塞状态的系统调用时接收到信号，并为该信号设置了信号处理函数，
        则默认情况下系统调用将被中断，并且errno设置为EINTR。可以用sigaction函数为信号
        设置SA_RESTART标志以重启被该信号中断的系统调用。
        */
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }

        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            //sockfd == sigpipefd[0]是唯一的
            if( sockfd == listenfd )
            {   
               
                struct sockaddr_in client_address;
                memset(&client_address,0,sizeof(client_address));
                socklen_t client_addrlength = sizeof( client_address );
                
                /*压测BUG解析
                ET模式下需要循环读取直到eagain，否则压测会出bug,因为ET模式下只有新数据来的时候才会通知，高并发情况下，
                一次性来了很多连接，但是只通知了一次，取出一个连接后就不再取了，此时全连接队列就一个空位，而且只有新连接
                来后才会通知，而新连接来之后全连接队列又满了，导致全连接队列经常处于满状态，从而导致半连接队列也一直满，无法接受新连接。
                解决办法：
                1.ET模式下需要循环读取直到eagain
                2.listenfd采用LT模式
                */
            
                //accept每次都返回未被使用的最小的文件描述符，因此一个连接断开后其文件描述符可以复用，因此可以用数组来描述用户
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                if ( connfd < 0 )
                {   
                    /*
                    if( errno == EAGAIN || errno == EWOULDBLOCK )
                    {
                        break;//ET模式下读取为空再跳出
                    }
                    */
                    printf( "errno is: %d\n", errno );
                    continue;
                }
                if( http_conn::m_user_count >= MAX_FD )
                {
                    show_error( connfd, "Internal server busy" );
                    continue;
                }
                LOG_INFO( "user:%d is connect", connfd);
                //初始化定时器
                Timer* timer = new Timer(3*TIMESLOT,connfd,cb_func);
                timer_lst.add_timer( timer );
                //初始化用户连接
                users[connfd].init( connfd, client_address, timer);
                
            }
            //sockfd == sigpipefd[0]是唯一的，所以用elseif
            else if( ( sockfd == sigpipefd[0] ) && ( events[i].events & EPOLLIN ) )//统一事件源信号处理
            {
                int sig;
                char signals[1024];
                memset(signals,0,sizeof(signals));
                ret = recv( sigpipefd[0], signals, sizeof(signals), 0 );
                if( ret == -1 )
                {
                    // handle the error
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGALRM://通过字符的ASCALL码进行对照
                            {
                                timeout=true;
                                break;
                            }
                            /*
                            case SIGINT:
                            {
                                stop=true;
                                break;
                            }
                            */
                        }
                    }
                }
            }
            else if( events[i].events & EPOLLRDHUP )
            {
                users[sockfd].close_conn();
                if(users[sockfd].m_timer)  timer_lst.del_timer(users[sockfd].m_timer);
            }
            else if( events[i].events & EPOLLIN )
            {
                if(0==mode)
                {
                    if( users[sockfd].read() )
                    {
                        pool->append(users+sockfd);//指针首地址加sockfd
                    }
                    else
                    {
                        users[sockfd].close_conn();
                        if(users[sockfd].m_timer)  timer_lst.del_timer(users[sockfd].m_timer);
                    }
                }   
                else
                {
                    users[sockfd].m_state=0;
                    pool->append(users+sockfd);//指针首地址加sockfd
                }
                
            }
            else if( events[i].events & EPOLLOUT )
            {
                if(0==mode)
                {
                    if( !users[sockfd].write() )
                    {
                        users[sockfd].close_conn();
                        if(users[sockfd].m_timer)  timer_lst.del_timer(users[sockfd].m_timer);
                    }
                }
                else
                {
                    users[sockfd].m_state=1;
                    pool->append(users+sockfd);//指针首地址加sockfd
                }
            }
           
        }
        //定时器任务优先级较低，放epoll事件循环结束后处理
        if(timeout){
            timer_handler();
            timeout=false;
        }
    }
    close( epollfd );
    close( listenfd );
    delete [] users;
    pool->stop();
    return 0;

}
