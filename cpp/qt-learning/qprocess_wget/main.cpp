#include <QCoreApplication>

#include "wget.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    Wget wget;
    wget.url = "repo:com.qq.weixin.work.deepin/4.0.0.6007/x86_64";
    wget.download();
    
    return a.exec();
}