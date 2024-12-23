#pragma once
#include"ServerApp.h"
#include <QtWidgets/QApplication>
int main(int argc, char* argv[])
{
   QApplication a(argc, argv);
    ServerApp App;
    App.Run();
    qDebug() << "MAIN " << QThread::currentThreadId();
    qDebug() << "MAIn " << QThread::currentThread();
    qDebug() << "HELLo ";
    return a.exec();
   
}