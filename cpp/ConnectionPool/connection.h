#pragma once
#include <string>
#include "public.h"
#include <mysql.h>
#include <ctime>
 
using namespace std;
// 数据库操作类 
class Connection
{
public:
 
	// 初始化数据库连接 
	Connection();
 
	// 释放数据库连接资源 
	~Connection();
	
	// 连接数据库
	bool connect(string ip, unsigned short port, string user, string password, string dbname);
	
	// 更新操作 insert、delete、update 
	bool update(string sql);
 
	// 查询操作 select 
	MYSQL_RES* query(string sql);
 
	//刷新连接的起始空闲时间
	void refreshAlivetime();
 
	//返回存活时间
	clock_t getAliveTime();
 
private:
	MYSQL *_conn; // 表示和MySQL Server的一条连接 
	clock_t _alivetime;
};