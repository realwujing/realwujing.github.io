#include <QCoreApplication>
#include <QFile>
#include <unistd.h>
#include <QDateTime>
#include <QTextStream>

void write_log_to_file(QString msg)
{
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString current_date = QString("%1").arg(current_date_time);
    QString message = QString("%1 %2").arg(current_date).arg(msg);
    QFile file("/home/wujing/code/cpp-learning/thread/QThread/test.text");
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream textStream(&file);
    textStream << message << "\n";
    file.flush();
    file.close();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	while(true)
	{
		write_log_to_file("hello start");
		sleep(2);
		write_log_to_file("hello end");
	}
    return 0;
    // return a.exec();
}
