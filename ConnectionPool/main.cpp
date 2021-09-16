#pragma once
#include <string>
#include <ctime>
#include "connection.h"
#include "connectionPool.h"
int main()
{
 
	 clock_t begin = clock();
	// ConnectionPool* p = ConnectionPool::getConnectionPool();
	 //单线程测试
#if 0
	 for (int i = 0; i < 1000; i++)
	 {
		 //通过数据库连接池进行连接
		shared_ptr<Connection> conn = p->getConnetcion();
		 char sql[1024] = { 0 };
		 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
		 "lisi", "w", 20);
		 conn->update(sql);
		 //普通连接
		 /*Connection conn;
		 char sql[1024] = { 0 };
		 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
		 "lisi", "w", 20);
		 conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
		 conn.update(sql); */
	 }
 
#endif
	 //多线程测试
	 thread t1([]()
	 {
		 ConnectionPool* p = ConnectionPool::getConnectionPool();
		 for (int i = 0; i < 250; i++)
		 {
			 //通过数据库连接池进行连接
			shared_ptr<Connection> conn = p->getConnetcion();
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
				 "lisi", "w", 20);
			 conn->update(sql); 
			 //普通连接
			 /* Connection conn;
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
			 "lisi", "w", 20);
			 conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
			 conn.update(sql);*/
		 }
	 });
 
	 thread t2([]()
	 {
		 ConnectionPool* p = ConnectionPool::getConnectionPool();
		 
		 for (int i = 0; i < 250; i++)
		 {
			 //通过数据库连接池进行连接
			shared_ptr<Connection> conn = p->getConnetcion();
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
				 "lisi", "w", 20);
			 conn->update(sql);
			 //普通连接
			 /*Connection conn;
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
			 "lisi", "w", 20);
			 conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
			 conn.update(sql);*/
		 }
	 });
 
	 thread t3([]()
	 {
		 ConnectionPool* p = ConnectionPool::getConnectionPool();
		 for (int i = 0; i < 250; i++)
		 {
			 //通过数据库连接池进行连接
			 shared_ptr<Connection> conn = p->getConnetcion();
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
				 "lisi", "w", 20);
			 conn->update(sql);
			 //普通连接
			 /* Connection conn;
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
			 "lisi", "w", 20);
			 conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
			 conn.update(sql);*/
		 }
	 });
 
	 thread t4([]()
	 {
		 ConnectionPool* p = ConnectionPool::getConnectionPool();
		 for (int i = 0; i < 250; i++)
		 {
			 //通过数据库连接池进行连接
			 shared_ptr<Connection> conn = p->getConnetcion();
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
				 "lisi", "w", 20);
			 conn->update(sql);
			 //普通连接
			 /*Connection conn;
			 char sql[1024] = { 0 };
			 sprintf(sql, "insert into users(name,sex,age) values('%s','%s',%d)",
			 "lisi", "w", 20);
			 conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
			 conn.update(sql);*/
		 }
	 });
 
	 t1.join();
	 t2.join();
	 t3.join();
	 t4.join();
	 clock_t end = clock();
 
	 cout << end - begin << "ms" << endl;
	 return 0;
}