#include "work_queue.h"
#include <iostream>
using namespace std;
int main(){

work_queue<int> q(6);
q.pop();
q.push(3);
q.push(4);
q.push(5);
q.push(1);
q.push(2);


cout<<q.size()<<endl;
cout<<q.front()<<endl;
cout<<q.back()<<endl;

q.push(8);
q.push(9);
q.push(6);
q.push(7);


cout<<q.size()<<endl;
cout<<q.front()<<endl;
cout<<q.back()<<endl;
return 0;
}