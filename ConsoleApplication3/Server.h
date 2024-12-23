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
#include <QtCore/qobject.h>
#include<QtCore/qthread.h>
#include"login.h"
#include<string>
#include"ServerFunction.h"
#include<iostream>
#include"ChatUI.h"
inline QString getRequest1(const QString& S) {
    QString L = ": ";
    qsizetype Pos = S.indexOf(L);
    return S.mid(Pos + 2);
}
inline void PRT() {
    qDebug() << "Current thread in PRT:" << QThread::currentThread();
    std::cout << "HELLO PRT" << std::endl;
    qDebug() << QThread::currentThread(); // 
}
class ServerThread;


class SocketWrapper : public QObject {
    Q_OBJECT
public:
    void HandleRequest(const QString& Request) {
        if (Request == "runApp") {

        }
        else if (Request == "") {}
    }
    explicit SocketWrapper(QTcpSocket* socket, QObject* parent = nullptr)
        : QObject(parent), m_socket(socket) {}

signals:
    void ReadyReceiveFile(QTcpSocket* Sock, const QString& FIlename);
    void PassMessage(const QString& S);
    void Reconnect();
public slots:
    Q_INVOKABLE void writeData(QString S) {
        if (m_socket) {
            QByteArray Data = S.toUtf8();
            m_socket->write(Data);
            qDebug() << "Socket";
        }
    }
    Q_INVOKABLE void writeData(const QByteArray& data) {
        if (m_socket) {
            m_socket->write(data);
            qDebug() << "Socket";
        }
    }
    Q_INVOKABLE void SendFile(QTcpSocket* socket, ServerThread* Server, const QString& file_path,int BPI = 2048) {
        if (!socket || !socket->isOpen()) {
            qDebug() << "Socket is not connected.";
            return;
        }

        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open file:" << file.errorString();
            return;
        }

        // 
        quint32 fileSize = static_cast<quint32>(file.size());

        // 
        QByteArray sizeBuffer;
        QDataStream sizeStream(&sizeBuffer, QIODevice::WriteOnly);
        sizeStream << fileSize;

        if (socket->write(sizeBuffer) == -1) {
            qDebug() << "Failed to send file size:" << socket->errorString();
            file.close();
            return;
        }


        const qint64 chunkSize = BPI; // 
        QByteArray buffer(chunkSize, 0); // 
        qint64 bytesSent = 0;
        char Check[4] = "";
        while (!file.atEnd()) {
            qint64 bytesRead = file.read(buffer.data(), chunkSize); // 
            if (bytesRead == -1) {
                qDebug() << "Failed to read from file:" << file.errorString();
                file.close();
                return;
            }

            qint64 sent = 0;
            int j = 0;
            while (sent < bytesRead) {
                qint64 result = socket->write(buffer.constData() + sent, bytesRead - sent); // 
                if (result == -1) {
                    qDebug() << "Failed to send file data:" << socket->errorString();
                    file.close();
                    return;
                }
                j++;
                std::cout << j << std::endl;
                sent += result;
            }
            socket->waitForReadyRead();
            socket->read(Check, 4);
            bytesSent += bytesRead;
            QThread::msleep(50);
        }

        socket->flush(); //
        file.close();
        qDebug() << "Sent file to client successfully. Total bytes sent:" << bytesSent;
        emit Reconnect();
    }
    Q_INVOKABLE void WebcamRecord(QTcpSocket*Sock,ServerThread*Server) {
        char S[] = "Enter number of seconds ";
        QByteArray Data = QByteArray(S);
        Sock->write(Data);
        Sock->waitForReadyRead();
        Sock->read(S, 25);
        int N = atoi(S);
        QString S1 = QString("Number of Seconds : %1").arg(N);
        emit PassMessage(S1);
        ServerFunction A;
        A.RecordVideoFromWebcam("WebcamRecord.avi", N);
        QString Filename = "WebcamRecord.avi";
        SendFile(Sock, Server, Filename,1024*512);


    }
    Q_INVOKABLE void ListApp(QTcpSocket* Sock) {
        std::vector<ProcessInfo> ListService = GetRunningApps();
        uint32_t NumberOfServices = ListService.size();
        qDebug() << "Number Of Services : " << NumberOfServices;
        QByteArray Data;
        QDataStream DataStream(&Data, QIODevice::WriteOnly);
        DataStream << NumberOfServices;
        if (Sock->write(Data) == -1) {
            qDebug() << "Failed to send file size:" << Sock->errorString();

            return;
        }
        QDataStream out(Sock);
        for (int i = 0; i < NumberOfServices; i++) {
            char c[16] = "";
            std::cout << i << std::endl;
            Sock->waitForReadyRead();
            Sock->read(c, 16);
            uint32_t ID = ListService[i].processID;
            QString Name = QString::fromStdWString(ListService[i].exeName);
            out << ID << Name;
        }
        emit Reconnect();
    }
    Q_INVOKABLE void ListService(QTcpSocket* Sock) {
        std::vector<ServiceInfo> ListService = GetServicesList() ;
        uint32_t NumberOfServices = ListService.size();
        qDebug() << "Number Of Services : " << NumberOfServices;
        QByteArray Data;
        QDataStream DataStream(&Data, QIODevice::WriteOnly);
        DataStream << NumberOfServices;
        if (Sock->write(Data) == -1) {
            qDebug() << "Failed to send file size:" << Sock->errorString();
         
            return;
        }
        QDataStream out(Sock);
        for (int i = 0; i < NumberOfServices; i++) {
            char c[16] = "";
            std::cout << i << std::endl;
            Sock->waitForReadyRead();
            Sock->read(c, 16);
            uint32_t Status = ListService[i].serviceStatus;
            QString DisplayName = QString::fromStdWString(ListService[i].displayName);
            QString ServiceName = QString::fromStdWString(ListService[i].serviceName);
            out << ServiceName << DisplayName << Status;
        }
        QString END = "\n";
        emit Reconnect();
    }
    Q_INVOKABLE void StopService(QTcpSocket*Sock) {
        std::vector<ServiceInfo> ListService = GetServicesList();
        uint32_t NumberOfServices = ListService.size();
        qDebug() << "Number Of Services : " << NumberOfServices;
        QByteArray Data;
        QDataStream DataStream(&Data, QIODevice::WriteOnly);
        DataStream << NumberOfServices;
        if (Sock->write(Data) == -1) {
            qDebug() << "Failed to send file size:" << Sock->errorString();

            return;
        }
        QDataStream out(Sock);
        for (int i = 0; i < NumberOfServices; i++) {
            char c[16] = "";
            std::cout << i << std::endl;
            Sock->waitForReadyRead();
            Sock->read(c, 16);
            uint32_t Status = ListService[i].serviceStatus;
            QString DisplayName = QString::fromStdWString(ListService[i].displayName);
            QString ServiceName = QString::fromStdWString(ListService[i].serviceName);
            out << ServiceName << DisplayName << Status;
        }
        Sock->waitForReadyRead();
        QString X;
        out >> X;
        out << QString("Enter Service ");
        QString ServiceName;
        Sock->waitForReadyRead();
        out >> ServiceName;
        QString MessageToGUI = QString("Service Name: ") + ServiceName;
        emit PassMessage(MessageToGUI);
        std::wstring wServiceName = ServiceName.toStdWString();
        bool Check = StopServiceByName(wServiceName);
        QString MESS;
        if (Check) {
            MESS = QString("Service %1 has stopped sucessfully").arg(ServiceName);
        }
        else {
            MESS = QString("Failed to stop service.");
        }
        out << MESS;
        emit PassMessage(MESS);
        emit Reconnect();
    }
    Q_INVOKABLE void StartService(QTcpSocket* Sock) {
        std::vector<ServiceInfo> ListService = GetServicesList();
        uint32_t NumberOfServices = ListService.size();
        qDebug() << "Number Of Services : " << NumberOfServices;
        QByteArray Data;
        QDataStream DataStream(&Data, QIODevice::WriteOnly);
        DataStream << NumberOfServices;
        if (Sock->write(Data) == -1) {
            qDebug() << "Failed to send file size:" << Sock->errorString();

            return;
        }
        QDataStream out(Sock);
        for (int i = 0; i < NumberOfServices; i++) {
            char c[16] = "";
            std::cout << i << std::endl;
            Sock->waitForReadyRead();
            Sock->read(c, 16);
            uint32_t Status = ListService[i].serviceStatus;
            QString DisplayName = QString::fromStdWString(ListService[i].displayName);
            QString ServiceName = QString::fromStdWString(ListService[i].serviceName);
            out << ServiceName << DisplayName << Status;
        }
        Sock->waitForReadyRead();
        QString X;
        out >> X;
        out << QString("Enter Service ");
        QString ServiceName;
        Sock->waitForReadyRead();
        out >> ServiceName;
        QString MessageToGUI = QString("Service Name: ") + ServiceName;
        emit PassMessage(MessageToGUI);
        std::wstring wServiceName = ServiceName.toStdWString();
        bool Check = StopServiceByName(wServiceName);
        QString MESS;
        if (Check) {
            MESS = QString("Service %1 has Started sucessfully").arg(ServiceName);
        }
        else {
            MESS = QString("Failed to stop service.");
        }
        out << MESS;
        emit PassMessage(MESS);
        emit Reconnect();
    }
    Q_INVOKABLE void ShutDown(QTcpSocket* Sock) {

    }
private:
    QTcpSocket* m_socket;
};


class MainWindow :public QWidget {
    Q_OBJECT
private slots:
    void toLoginPage() {
        Page->setCurrentIndex(0);

    }
    void toChatPage() {
        Page->setCurrentIndex(1);
    }

private:
    QTcpServer* Sock = NULL;
    QStackedWidget* Page = NULL;
    LoginWidget* loginPage = NULL;
    ChatWidget* ChatPage = NULL;
    void ReadInput() {
        QString S = ChatPage->InputText->text();
        if (S == "") {
            return;
        }
        UpdateMessage(S);
        ChatPage->InputText->clear();
    }
    MainWindow() {
        setGeometry(QRect(0, 0, 1000, 800));
        Page = new QStackedWidget(this);
        Page->setGeometry(QRect(0, 0, 1000, 800));
        loginPage = new LoginWidget();
        ChatPage = new ChatWidget();
        Page->addWidget(loginPage);
        Page->addWidget(ChatPage);
        connect(loginPage->LoginButton, &QPushButton::clicked, this, &MainWindow::toChatPage);
        connect(ChatPage->BackButton, &QPushButton::clicked, this, &MainWindow::toLoginPage);
        connect(ChatPage->SendButton, &QPushButton::clicked, this, &MainWindow::ReadInput);
    }
protected:
    void closeEvent(QCloseEvent* event) override {

        emit stopServer();
        event->accept();
    }
    void UpdateMessage(const QString& S) {
        ChatPage->Update(S);
    }

signals:
    void stopServer();
    friend class ServerThread;
    friend class ServerApp;
};

class ServerThread : public QThread {
    Q_OBJECT
private:
    QTcpServer* Sock;
    MainWindow* UI;
    QTcpSocket* Socket = nullptr;
    SocketWrapper* Wrapper = NULL;
    ServerFunction *Function;
    QString S = "";
ServerThread(MainWindow* mainUI, QObject* parent = nullptr)
{
    UI = mainUI;
    Sock = new QTcpServer();
    Function = new ServerFunction();
    Sock->moveToThread(this);
    Function->moveToThread(this);
    std::cout << "CHILD" << std::endl;
    connect(Sock, &QTcpServer::newConnection, this, &ServerThread::onNewConnection);

}
void run() override {


    connect(this, &ServerThread::PassMessage, UI, &MainWindow::UpdateMessage);
    Function->TakeScreenshot("Screenshot.jpg");
    ServerFunction A;
    std::string S1 = "screenshot.jpg";
    A.TakeScreenshot(S1);
    qDebug() << "SERVER " << currentThreadId();
    qDebug() << "SERVER " << currentThread();
    if (Sock->listen(QHostAddress::Any, 8080)) {
        emit serverStarted("Server is now running on port 8000!");
        emit PassMessage("Sever is Listening");

    }
    else {
        emit serverStarted("Server failed to start.");
        emit PassMessage("Server failed to start.");
        return;
    }
    exec();
}
void Reconnect() {
    connect(Socket, &QTcpSocket::readyRead, this, &ServerThread::receiveData);
}

void HandleRequest1(QTcpSocket* Sock, const QString& Request) {
    if (Request == "runApp") {
    }
    else if (Request == "Take screenshot") {
        QString S = "Screenshot1.bmp";
        std::string S1 = "Screenshot1.bmp";
        qDebug() << "CURRENTTTTTTT: " << currentThread();
        ServerFunction A;
        A.TakeScreenshot(S1);
        QMetaObject::invokeMethod(Wrapper, "SendFile", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket), Q_ARG(ServerThread*, this), Q_ARG(const QString&, S));
    }
    else if (Request == "Webcam Capture") {
        QString S = "WebcamImage.jpg";
        std::string S1 = "WebcamImage.jpg";
        ServerFunction A;
        A.CaptureWebcamImage(S1);
        QMetaObject::invokeMethod(Wrapper, "SendFile", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket), Q_ARG(ServerThread*, this), Q_ARG(const QString&, S));

    }
    else if (Request == "Webcam Record") {
        QMetaObject::invokeMethod(Wrapper, "WebcamRecord", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket), Q_ARG(ServerThread*, this));
    }
    else if (Request == "list") {}
    else if (Request == "getListApps") {}
    else if (Request == "shutdown") {}
    else if (Request == "Exit") {}
    else if(Request == "List Service"){
        QMetaObject::invokeMethod(Wrapper, "ListService", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket));

    }
    else if(Request =="Stop Service"){
        QMetaObject::invokeMethod(Wrapper, "StopService", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket));
    }
    else if (Request == "Start Service") {
        QMetaObject::invokeMethod(Wrapper, "StartService", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Socket));
    }
    else {
        qDebug() << "THREAD IS " << currentThread();
        //ServerFunction(A);
        //QString S = "Screenshot1.bmp";
        //std::string S1 = "Screenshot1.bmp";
        //A.TakeScreenshot(S1);
        Reconnect();
    }
}
signals:
    void serverStarted(QString message);
    void newMessageReceived(const QString& message);
    void PassMessage(const QString& S);
    void StopServer();
private slots:
    void stop() {
        std::wstring L = L"abc";
        qDebug() << "Stopping server thread";
        Sock->close();
        quit();
    }

    void onNewConnection() {
        Socket = Sock->nextPendingConnection();
        Wrapper = new SocketWrapper(Socket);
        Socket->moveToThread(this);
        Wrapper->moveToThread(this);
        connect(Wrapper, &SocketWrapper::PassMessage, UI, &MainWindow::UpdateMessage);
        connect(Socket, &QTcpSocket::readyRead, this, &ServerThread::receiveData);
        connect(Socket, &QTcpSocket::disconnected, Socket, &QTcpSocket::deleteLater);
        connect(this, &ServerThread::newMessageReceived, UI, &MainWindow::UpdateMessage);
        connect(this, &ServerThread::StopServer, Sock, &QTcpServer::close);
        connect(this, &ServerThread::finished, Sock, &QTcpServer::close);
        connect(this, &ServerThread::finished, Sock, &QTcpServer::deleteLater);
        connect(Wrapper, &SocketWrapper::Reconnect, this, &ServerThread::Reconnect);
    }

    
private slots:
    void DeleteSocket() {

        QMetaObject::invokeMethod(Sock,"close",Qt::QueuedConnection);
        std::cout << "SOCKET IS DELETED" <<  std::endl;
    }
private:
    void receiveData() {
        QByteArray data = Socket->read(1024);
        QString message = QString::fromUtf8(data);
        QString Request = getRequest1(message);
        qDebug() << "Current Thread is "<<this->thread();
        qDebug() << "Wrapper Thread is " << Wrapper->thread();
        qDebug() << "Currentt Thread is " << QThread::currentThread();
        emit newMessageReceived(message);
        disconnect(Socket, &QTcpSocket::readyRead, this, &ServerThread::receiveData);
        HandleRequest1(Socket, Request);
    }
public:
    friend class ServerApp;
    ~ServerThread() {
        UI = NULL;
        delete Function;
        delete Socket;
        delete Sock;
        
    }
};
