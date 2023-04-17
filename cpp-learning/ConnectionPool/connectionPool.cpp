#pragma once
#include "connectionPool.h"
 
ConnectionPool* ConnectionPool:: getConnectionPool()
{
	static ConnectionPool pool;//luck或unlock
	return &pool;
}
 
bool ConnectionPool::loadConfigFile()
{
	FILE *pf = fopen("mysql.ini", "r");
	if (pf == nullptr)
	{
		LOG("mysql.ini file is not exist!");
		return false;
	}
 
	while (!feof(pf))
	{
		char line[1024] = { 0 };
		fgets(line, 1024, pf);
		string str = line;
		int idx = str.find('=', 0);
		if (idx == -1)//无效的配置项
		{
			continue;
		}
 
		int endidx = str.find('\n', idx);
 
		string key = str.substr(0, idx);
		string value = str.substr(idx + 1, endidx - idx - 1);
 
		if (key == "ip")
		{
			_ip = value;
		}
		else if (key == "port")
		{
			_port = atoi(value.c_str());
		}
		else if (key == "username")
		{
			_username = value;
		}
		else if (key == "password")
		{
			_password = value;
		}
		else if (key == "dbname")
		{
			_dbname = value;
		}
		else if (key == "initSize")
		{
			_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")
		{
			_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdexTime")
		{
			_maxIdleTime = _connectionTimeuot;
		}
		else if (key == "maxconnectionTimeout")
		{
			_connectionTimeuot = _connectionTimeuot;
		};
	}
	return true;
}
 
 
	//连接池的构造
	ConnectionPool:: ConnectionPool()
	{
		if (!loadConfigFile())
		{
			return;
		}
		//创建初始的连接
 
		for (int i = 0; i < _initSize; i++)
		{
			Connection *p = new Connection();
			p->connect(_ip, _port, _username, _password, _dbname);
			_ConnectionQue.push(p);
			p->refreshAlivetime();
			_connectionCnt++;
		}
 
 
		//启动一个新的线程，作为连接的生产者
		thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
		produce.detach();
		
		//启动一个新的线程，扫描超过maxidletime时间的空闲连接，进行队列的回收
		thread sanner(std::bind(&ConnectionPool::sannerConnectionTask, this));
		sanner.detach();
	}
 
	void ConnectionPool::produceConnectionTask()
	{
		for (;;)
		{
			unique_lock<mutex> lock(_queueMutex);
			while (!_ConnectionQue.empty())
			{
				cv.wait(lock);//如果队列不为空，条件变量处于阻塞状态
			}
			
			//如果队列为空，就得创建新的连接
			if (_connectionCnt < _maxSize)
			{
				Connection *conn = new Connection();
				conn->connect(_ip, _port, _username, _password, _dbname);
				
				conn->refreshAlivetime();
				_ConnectionQue.push(conn); //将新连接入队
				_connectionCnt++;
			}
			cv.notify_all(); //通知消费者线程可以获取连接了;
		}
	}
 
	//给用户提供一个接口专门负责用户获取线程
	shared_ptr<Connection> ConnectionPool::getConnetcion()
	{
		unique_lock<mutex> lock(_queueMutex);  //给queue加锁
		
		while (_ConnectionQue.empty())
		{
			// sleep
			if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeuot)))
			{
				if (_ConnectionQue.empty())
				{
					LOG("获取空闲连接超时了...获取连接失败!");
					return nullptr;
				}
			}
		}
 
		//对于shared_ptr而言，出作用域后自动析构是将连接直接销毁，而我们要达到的目的是
		//要将连接归还，所以要自定义删除器,使用lamboda表达式来定义删除器
		shared_ptr<Connection> sp(_ConnectionQue.front(),
			[&](Connection *pcon) {
				//这里是服务器应用线程调用的，所以一定要注意线程安全问题
			unique_lock<mutex> lock(_queueMutex);
			pcon->refreshAlivetime();
			_ConnectionQue.push(pcon);
		});
 
		_ConnectionQue.pop();
		cv.notify_all(); //通知消费者线程检查一下队列是否为空
		return sp;
	}
 
 
	//用来对队列进行扫描的线程函数
	void ConnectionPool::sannerConnectionTask()
	{
		for (;;)
		{
			//通过sleep来模拟定时效果
			this_thread::sleep_for(chrono::seconds(_maxIdleTime));
 
			//扫描整个队列，释放空闲连接
			unique_lock<mutex> lock(_queueMutex);
			while (_connectionCnt > _initSize)
			{
				Connection *p = _ConnectionQue.front();
 
			//如果说连接空闲时间超过了最大空闲时间，就将连接出队，然后delete，
			//当然了，这里得记录连接存活的时间
				if (p->getAliveTime() > (1000 * _maxIdleTime)) //如果大于最大闲时间就得出队列
				{
					_ConnectionQue.pop();
					_connectionCnt--;
					delete p; //调~Connection()将资源释放即可
				}
				else
				{
					break; //如果说队头元素未超时，则其他元素也未超时
				}
			}
		}
	}