/*** 
 * @Author: wujing
 * @Date: 2021-01-21 16:21:08
 * @LastEditTime: 2021-01-21 16:23:34
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /wujing/CPlusPlusProject/C++ 信号处理/main.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <iostream>
#include <csignal>
#include <unistd.h>
 
using namespace std;
 
void signalHandler( int signum )
{
    cout << "Interrupt signal (" << signum << ") received.\n";
 
    // 清理并关闭
    // 终止程序 
 
   exit(signum);  
 
}
 
int main ()
{
    int i = 0;
    // 注册信号 SIGINT 和信号处理程序
    signal(SIGINT, signalHandler);  
 
    while(++i){
       cout << "Going to sleep...." << endl;
       if( i == 3 ){
          raise( SIGINT);
       }
       sleep(1);
    }
 
    return 0;
}