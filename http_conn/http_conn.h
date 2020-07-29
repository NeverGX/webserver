#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "../utils/utils.h"
#include "../timer/heap_timer.h"

class http_conn
{
public:
    //设置读取文件的名称m_real_file大小
    static const int FILENAME_LEN = 200;
    //设置读缓冲区m_read_buf大小
    static const int READ_BUFFER_SIZE = 2048;
    //设置写缓冲区m_write_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;
    
    //报文的请求方法，本项目只用到GET和POST
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    //主状态机的状态
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
     //从状态机的状态
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    //报文解析的结果
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
   
public:
    http_conn(){}
    ~http_conn(){}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init( int sockfd, const sockaddr_in& addr, Timer *timer);
    //关闭http连接
    void close_conn();
    void process();
    //读取浏览器端发来的全部数据
    bool read();
    //响应报文写入函数
    bool write();

private:
    void init();

    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write( HTTP_CODE ret );

    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line( char* text );
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers( char* text );
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content( char* text );
    //生成响应报文
    HTTP_CODE do_request();

    //m_start_line是已经解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char* get_line() { return m_read_buf + m_start_line; }
    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    void unmap();
    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    Timer* m_timer;//定时器
    int m_state;  //状态读为0，写为1
    int m_mode;

private:
    int m_sockfd;
    sockaddr_in m_address;

    //读缓存区
    char m_read_buf[ READ_BUFFER_SIZE ];
    //m_read_buf读入数据的最后一个字节的下一个位置
    int m_read_idx;
    //当前正在分析的字符串在m_read_buf中的为位置
    int m_checked_idx;
    //当前正在解析的行的起始位置
    int m_start_line;

    //写缓冲区
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    //待发送数据的起始位置
    int m_write_idx;

    //主状态机当前所处的状态
    CHECK_STATE m_check_state;
    //请求的方法
    METHOD m_method;

    //以下为解析请求报文中对应的6个变量
    //存储读取文件的名称
    char m_real_file[ FILENAME_LEN ];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;

    char* m_file_address;//读取服务器上的文件地址
    int bytes_to_send;
    struct stat m_file_stat;
    struct iovec m_iv[2];//io向量机制iove
    int m_iv_count;
};

#endif