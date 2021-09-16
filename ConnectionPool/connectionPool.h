#pragma once
 
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <condition_variable>
#include <functional>
#include "Connection.h"
/*
	实现连接池功能模块
*/
 
using namespace std;
class ConnectionPool
{
public:
	//获取连接池对象实例
	static ConnectionPool* getConnectionPool();
	shared_ptr<Connection> getConnetcion();  //用户获得连接的接口
private:
	ConnectionPool();//单例模式，将构造函数私有化
 
 
	//从配置文件中加载配置项
	bool loadConfigFile();
 
	//单独运行一个线程，用于生产连接
	void produceConnectionTask();
 
 
	//单独运行一个线程，用于删除扫描队列中超过maxidletime连接
	void sannerConnectionTask();
 
	string _ip; //mysql登录ip地址
	unsigned short _port; // mysql端口号3306
	string _username; // mysql登录用户名
	string _password; //  mysql登录密码
	string _dbname; // 连接的数据库名称
	int _initSize; //连接池连接初始量
	int _maxSize; //连接池最大连接量
	int _maxIdleTime; //连接池最大空闲时间
	int _connectionTimeuot; // 最大超时时间
 
	queue<Connection*> _ConnectionQue; // 储存mysql连接的队列
	mutex _queueMutex; // 维护连接队列的线程安全互斥锁
	atomic_int _connectionCnt; //记录所创建的connection连接的总的个数
	condition_variable cv; //用于生产者线程和消费者线程之间的通信		
};