#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include <QtNetwork/qtcpsocket.h>
#include<QtNetwork/qtcpserver.h>
#include <QtCore/QObject>
#include<QtCore/qthread.h>
#include<string>
#include<iostream>
#include"Server.h"
#include "ServerFunction.h"
class ServerApp {
private:
	MainWindow* UI = NULL;
	ServerThread* Threading = NULL;
public:
	ServerApp() {
	
		UI = new MainWindow();
		Threading = new ServerThread(UI);
		qDebug() << "Threading " << Threading->thread();
		qDebug() << "UI " << UI->thread();

	}
	void Run() {
		Threading->start();
		
		UI->show();
		

	}
	~ServerApp() {
		delete UI;
		if (Threading->isRunning()) {
			qDebug() << "Socket thread is still running.";
			
			Threading->quit();
			Threading->wait();
	
			if (Threading->isRunning()) {
				qDebug() << "Socket thread is still running.";
			}
			else {
				qDebug() << "Socket thread has stopped.";
				
				delete Threading;
			}
			std::cout << "HELLO" << std::endl;
		}
		else {
			qDebug() << "Socket thread has stopped.";
			delete Threading;
		}
	}
};