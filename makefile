cxx=g++
project=server
src= main.cpp ./http_conn/http_conn.cpp ./log/log.cpp ./utils/utils.cpp 
header= ./lock/locker.h ./timer/heap_timer.h ./utils/utils.h ./http_conn/http_conn.h ./log/log.h ./threadpool/threadpool.h
obj= main.o ./http_conn/http_conn.o ./log/log.o ./utils/utils.o 
debug = 1
ifeq ($(debug), 1)
    cxxflags += -g
else
    cxxflags += -O2
endif

#  $<表示第一个依赖 $^表示所有的依赖 $@表示目标文件

$(project): $(obj) 
	$(cxx) -o $(project) $^ -lpthread

# 注意编译.o文件时，命令-o要放到-c后面,否则会编译出错，或者不用-o，它会自动生成对应的.o文件
# 调试选项(-g)要在生成可重定位文件(.o文件)时加上，而不是链接的时候

main.o: main.cpp $(header) 
	$(cxx) -c $<  $(cxxflags) -std=c++11

./http_conn/http_conn.o: ./http_conn/http_conn.cpp $(header) 
	$(cxx) -c $< -o $@ $(cxxflags) -std=c++11

./log/log.o: ./log/log.cpp $(header) 
	$(cxx) -c $< -o $@ $(cxxflags) -std=c++11

./utils/utils.o : ./utils/utils.cpp  $(header) 
	$(cxx) -c $< -o $@ $(cxxflags) -std=c++11

clean: 
	rm  -r $(obj)