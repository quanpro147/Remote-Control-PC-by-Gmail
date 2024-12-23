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
#include"ChatUI.h"
#include"Login.h"
#include<QtCore/qthread.h>
#include<string>
#include<iostream>
#include<thread>
#include<chrono>
#include "ClientFunction.h"
struct ProcessInfo {
    uint32_t processID;
    QString exeName;
};


inline QString getRequest(const QString& S) {
    QString L = ": ";
    qsizetype Pos = S.indexOf(L);
    return S.mid(Pos + 2);
}

class MainWindow :public QWidget {
    Q_OBJECT
private :
    void toLoginPage() {
        Page->setCurrentIndex(0);

    }
    void toChatPage() {
        Page->setCurrentIndex(1);
    }
    void CanInput() {
        ChatPage->SendButton->show();
        ChatPage->InputText->show();
    }
    void UpdateMessage(const QString& S) {
        ChatPage->Update(S);
    }
    void stopServer() {
        std::cout << "HELLO" << std::endl;
    }
    
    void UpdateProgress(const QString& S) {
        QTextCursor cursor = ChatPage->PlainText->textCursor();


        cursor.movePosition(QTextCursor::End);

     
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);

        cursor.removeSelectedText();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.insertText(S);
    }
    void HideButton() {
        ChatPage->SendNumber->hide();
        ChatPage->InputNumber->hide();
    }
    void ShowButton() {
        ChatPage->SendNumber->show();
        ChatPage->InputNumber->show();
    }
    void HideTextInput() {
        ChatPage->SendButton->hide();
        ChatPage->InputText->hide();
    }
    void ShowTextInput() {
        ChatPage->SendButton->show();
        ChatPage->InputText->show();
    }
    friend class ClientThread;
    friend class ClientApp;
private:
    QStackedWidget* Page = NULL;
    LoginWidget* loginPage = NULL;
    ChatWidget* ChatPage = NULL;
public:
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

    }
    
    
protected:
    void closeEvent(QCloseEvent* event) override {

        emit stopServer();
        event->accept();
    }
   
};
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
    void SendMessagetoGUI(const QString& S);
    void UpdateProgress(const QString& S);
    void UpdateMessage(const QString& S);
    void HideButton();
    void ShowButton();
    void HideTextInput();
    void ShowTextInput();
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
    Q_INVOKABLE void ReceiveFile(QTcpSocket* socket, ClientThread* Client, const QString& filename,/* Byte per Iteration*/int BPI = 2048) {
        if (!socket || !socket->isOpen()) {
            qWarning() << "Socket is not valid or not open.";
            return;
        }

       
        uint32_t fileSize = 0;
        QByteArray sizeBuffer;
        while (sizeBuffer.size() < static_cast<int>(sizeof(uint32_t))) {
            if (!socket->waitForReadyRead(30000)) {  // Timeout 3s
                qWarning() << "Timeout while waiting for file size.";
                return;
            }
            sizeBuffer.append(socket->read(static_cast<int>(sizeof(uint32_t)) - sizeBuffer.size()));
        }
        QDataStream sizeStream(sizeBuffer);
        sizeStream >> fileSize;
        QString PROGRESS = QString("Receiving file of the size : %1 Bytes").arg(fileSize);
        qDebug() << "Receiving file of size:" << fileSize << "bytes";
        emit UpdateMessage(PROGRESS);

        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open file for writing:" << file.errorString();
            return;
        }

        QByteArray fileBuffer;
        quint64 totalBytesReceived = 0;
        PROGRESS  = QString("Receive file %1/%2 bytes (%3%)")
            .arg(totalBytesReceived)
            .arg(fileSize)
            .arg(0, 0, 'f', 2);
        emit UpdateMessage(PROGRESS);
        
        char Check[] = "Ok";
        while (totalBytesReceived < fileSize) {
            socket->waitForReadyRead();

            //
            QByteArray chunk = socket->read(qMin(static_cast<qint64>(BPI), static_cast<qint64>(fileSize - totalBytesReceived)));
            file.write(chunk);  // 
            totalBytesReceived += chunk.size();
            socket->write(Check);
            double progress = (double)totalBytesReceived / fileSize * 100;
            QString progressText = QString("Receive file %1/%2 bytes (%3%)")
                .arg(totalBytesReceived)
                .arg(fileSize)
                .arg(progress, 0, 'f', 2);
                
            emit UpdateProgress(progressText);
        }

        file.close();
        qDebug() << "File received successfully and saved as:" << filename;
        PROGRESS = QString("File received successfully and saved as: %1").arg(filename);
        emit UpdateMessage(PROGRESS);
        QString END = "\n";
        emit UpdateMessage(END);

    }
    Q_INVOKABLE void WebcamRecord(QTcpSocket*Sock,ClientThread*Client ,QString*S,const QString &Filename) {
        char c[25];
        Sock->waitForReadyRead();
        Sock->read(c, 25);
        QString MESS = QString("Enter number of Seconds ");

        emit UpdateMessage(MESS);
        emit ShowButton();
        QThread::msleep(8000);
        emit HideButton();
        MESS = QString("Number of seconds : ") + *S + QString(" Seconds");
        emit UpdateMessage(MESS);
        QByteArray Data = (*S).toUtf8();
        Sock->write(Data);
        QThread::msleep(5000);
        ReceiveFile(Sock, Client, Filename,1024*512);
        

        




    }
    Q_INVOKABLE void ListApp(QTcpSocket* Sock) {
        qDebug() << "List App ";
        uint32_t NumberOfApp= 0;
        QByteArray Data;
        while (Data.size() < static_cast<int>(sizeof(uint32_t))) {
            if (!Sock->waitForReadyRead(30000)) {  // Timeout 3s
                qWarning() << "Timeout while waiting for file size.";
                return;
            }
            Data.append(Sock->read(static_cast<int>(sizeof(uint32_t)) - Data.size()));
        }

        QDataStream ServiceStream(Data);
        ServiceStream >> NumberOfApp;
        qDebug() << "HELLO";
        qDebug() << "Number Of App : " << NumberOfApp;
        QDataStream In(Sock);
        std::vector<ProcessInfo>ListService(NumberOfApp);
        for (int i = 0; i < NumberOfApp; i++) {

            Sock->write("Ok");
            Sock->waitForReadyRead();
            std::cout << i << std::endl;
            In >> ListService[i].processID >> ListService[i].exeName;
            QString MESS = QString("ProcessID: ") + QString::number(ListService[i].processID) + QString(" | Name: ") + ListService[i].exeName;
            emit UpdateMessage(MESS);
        }
        QString END = "\n";
        emit UpdateMessage(END);
        return;
    }
    Q_INVOKABLE std::vector<ServiceInfo> ListService(QTcpSocket* Sock) {
        qDebug() << "List Service ";
        uint32_t NumberOfService = 0;
        QByteArray Data;
        while (Data.size() < static_cast<int>(sizeof(uint32_t))) {
            if (!Sock->waitForReadyRead(30000)) {  // Timeout 3s
                qWarning() << "Timeout while waiting for file size.";
                return {};
            }
            Data.append(Sock->read(static_cast<int>(sizeof(uint32_t)) - Data.size()));
        }

        QDataStream ServiceStream(Data);
        ServiceStream >> NumberOfService;
        qDebug() << "HELLO";
        qDebug() << "Number Of Services : " << NumberOfService;
        QDataStream In(Sock);
        std::vector<ServiceInfo>ListService(NumberOfService);
        for (int i = 0; i < NumberOfService; i++) {

            Sock->write("Ok");
            Sock->waitForReadyRead();
            std::cout << i << std::endl;
            In >> ListService[i].serviceName >> ListService[i].displayName >> ListService[i].serviceStatus;
            QString MESS = QString("Name : %1 | DisplayName: %2 | ServiceStatus: %3").arg(ListService[i].serviceName)
                .arg(ListService[i].displayName).arg(ServiceStatusToString(ListService[i].serviceStatus));
            emit UpdateMessage(MESS);
        }
        QString END = "\n";
        emit UpdateMessage(END);
        return ListService ;



    }
    Q_INVOKABLE void StopService(QTcpSocket* Sock, QString* ID) {
       if(true) {
            qDebug() << "List Service ";
            uint32_t NumberOfService = 0;
            QByteArray Data;
            while (Data.size() < static_cast<int>(sizeof(uint32_t))) {
                if (!Sock->waitForReadyRead(30000)) {  // Timeout 3s
                    qWarning() << "Timeout while waiting for file size.";
                    return;
                }
                Data.append(Sock->read(static_cast<int>(sizeof(uint32_t)) - Data.size()));
            }

            QDataStream ServiceStream(Data);
            ServiceStream >> NumberOfService;
            qDebug() << "HELLO";
            qDebug() << "Number Of Services : " << NumberOfService;
            QDataStream In(Sock);
            std::vector<ServiceInfo>ListService(NumberOfService);
            for (int i = 0; i < NumberOfService; i++) {

                Sock->write("Ok");
                Sock->waitForReadyRead();
                std::cout << i << std::endl;
                In >> ListService[i].serviceName >> ListService[i].displayName >> ListService[i].serviceStatus;
                if (ListService[i].serviceStatus == 4) {
                    QString MESS = QString("Name : %1 | DisplayName: %2").arg(ListService[i].serviceName)
                        .arg(ListService[i].displayName);

                    emit UpdateMessage(MESS);
                }
            }
            QString END = "\n";
        }
        QDataStream In(Sock);
        In << QString("Ok");
        QString MESS;
        Sock->waitForReadyRead();
        In >> MESS;
    
        emit UpdateMessage(MESS);
        
        emit ShowTextInput();
        QThread::msleep(8000);
        emit HideTextInput();
        In << *ID;
        MESS = QString("Service : ") + *ID;
        emit UpdateMessage(MESS);
        
        QThread::msleep(5000);
        QString ServerResponse;
        Sock->waitForReadyRead();
        In >> ServerResponse;
        emit UpdateMessage(ServerResponse);
        QString END = "\n";
        emit UpdateMessage(END);

    }
    Q_INVOKABLE void StartService(QTcpSocket* Sock,QString*ID) {
        if (true) {
            qDebug() << "List Service ";
            uint32_t NumberOfService = 0;
            QByteArray Data;
            while (Data.size() < static_cast<int>(sizeof(uint32_t))) {
                if (!Sock->waitForReadyRead(30000)) {  // Timeout 3s
                    qWarning() << "Timeout while waiting for file size.";
                    return;
                }
                Data.append(Sock->read(static_cast<int>(sizeof(uint32_t)) - Data.size()));
            }

            QDataStream ServiceStream(Data);
            ServiceStream >> NumberOfService;
            qDebug() << "HELLO";
            qDebug() << "Number Of Services : " << NumberOfService;
            QDataStream In(Sock);
            std::vector<ServiceInfo>ListService(NumberOfService);
            for (int i = 0; i < NumberOfService; i++) {

                Sock->write("Ok");
                Sock->waitForReadyRead();
                std::cout << i << std::endl;
                In >> ListService[i].serviceName >> ListService[i].displayName >> ListService[i].serviceStatus;
                if (ListService[i].serviceStatus == 1) {
                    QString MESS = QString("Name : %1 | DisplayName: %2 ").arg(ListService[i].serviceName)
                        .arg(ListService[i].displayName);

                    emit UpdateMessage(MESS);
                }
            }
            QString END = "\n";
        }
        QDataStream In(Sock);
        In << QString("Ok");
        QString MESS;
        Sock->waitForReadyRead();
        In >> MESS;

        emit UpdateMessage(MESS);

        emit ShowTextInput();
        QThread::msleep(8000);
        emit HideTextInput();
        In << *ID;
        MESS = QString("Service : ") + *ID;
        emit UpdateMessage(MESS);

        QThread::msleep(5000);
        QString ServerResponse;
        Sock->waitForReadyRead();
        In >> ServerResponse;
        emit UpdateMessage(ServerResponse);
        QString END = "\n";
        emit UpdateMessage(END);
    }

    
    

private:
    QTcpSocket* m_socket;
};
class ClientThread : public QThread {
    Q_OBJECT
private:
    MainWindow* UI = NULL;
    QTcpSocket* Socket = nullptr;
    QString HOSTNAME = "";
    qint16 Port = 0;
    SocketWrapper* Wrapper = NULL;
    QString NumberOfSeconds = "0";
    QString ServiceName = "";
    ClientThread(const QString Hostname, qint16 PORT, MainWindow* ClientUI) {
        HOSTNAME = Hostname;
        Port = PORT;
        UI = ClientUI;
        Socket = new QTcpSocket();
        Wrapper = new SocketWrapper(Socket);
        Socket->moveToThread(this);
        Wrapper->moveToThread(this);
        
    }
    bool HandleRequest(QTcpSocket* Sock, QString& Request) {
        if (Request == "Run App") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Run App"));
        }
        else if (Request == "List App") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : List App"));
            
        }
        else if (Request == "Shutdown") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Shutdown"));
        }
        else if (Request == "Close") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Close"));
        }
       
        else if (Request == "Take screenshot") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Take screenshot"));
            QString Filename = "Screenshot.jpg";
            QMetaObject::invokeMethod(Wrapper, "ReceiveFile", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Sock),Q_ARG(ClientThread*, this), Q_ARG(const QString&, Filename));
            qDebug() << "Method is called";
        }
        else if (Request == "Webcam Record") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Webcam Record"));
            QString S = "WebcamRecord.avi";
            QMetaObject::invokeMethod(Wrapper, "WebcamRecord", Qt::QueuedConnection, Q_ARG(QTcpSocket*,Sock),Q_ARG(ClientThread*,this),Q_ARG(QString*,&NumberOfSeconds), Q_ARG(const QString&, S));
        }
        else if (Request == "Webcam Capture") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Webcam Capture"));
            QString Filename = "WebcamCapture.jpg";
            QMetaObject::invokeMethod(Wrapper, "ReceiveFile", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Sock), Q_ARG(ClientThread*, this), Q_ARG(const QString&, Filename));
            qDebug() << "Method is called";
        }
        else if (Request == "List Service") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : List Service"));
            QMetaObject::invokeMethod(Wrapper, "ListService", Qt::QueuedConnection, Q_ARG(QTcpSocket*,Sock));
         


        }
        else if (Request == "Stop Service") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Stop Service"));
            QMetaObject::invokeMethod(Wrapper, "StopService", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Sock),Q_ARG(QString*,&ServiceName));
        }
        else if (Request == "Start Service") {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Start Service"));
            QMetaObject::invokeMethod(Wrapper, "StartService", Qt::QueuedConnection, Q_ARG(QTcpSocket*, Sock), Q_ARG(QString*, &ServiceName));
        }

        else {
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QString, "Request : Invalid Request"));
        }
        return true;
    }
    void GetService() {
        ServiceName = UI->ChatPage->InputText->text();

    }
    void GetValuefromNumber() {
        NumberOfSeconds = UI->ChatPage->InputNumber->text();
        if (NumberOfSeconds == "") {
            QString MESS = "Default Second : 3 seconds ";
            UI->UpdateMessage(MESS);
            NumberOfSeconds = "3";
        }
        else {
            QString MESS = QString("Number of second : ") + NumberOfSeconds + QString(" seconds ");
            UI->UpdateMessage(MESS);
        }
    }
    void RunAppRequest() {
        QByteArray Data(1024, '\0');
        QString Index = QString::fromUtf8(Data);
        bool Ok = true;
        int AppIndex = Index.toInt(&Ok);

    }
    void run() override {
        connect(Socket, &QTcpSocket::connected, this, &ClientThread::onConnected);
        //connect(Socket, &QTcpSocket::readyRead, this, &ClientThread::ReadMessage);
        connect(Socket, &QTcpSocket::errorOccurred, this, &ClientThread::onError);
        connect(this, &ClientThread::SendMessagetoGUI, UI, &MainWindow::UpdateMessage);
        connect(Wrapper, &SocketWrapper::SendMessagetoGUI, UI, &MainWindow::UpdateMessage);
        connect(Socket, &QTcpSocket::disconnected, Socket, &QTcpSocket::deleteLater);
        connect(Wrapper, &SocketWrapper::UpdateProgress,UI, &MainWindow::UpdateProgress);
        connect(Wrapper, &SocketWrapper::UpdateMessage, UI, &MainWindow::UpdateMessage);
        connect(UI->ChatPage->SendButton, &QPushButton::clicked, this, &ClientThread::GetService);
        connect(UI->ChatPage->SendNumber, &QPushButton::clicked, this, &ClientThread::GetValuefromNumber);
        connect(Wrapper, &SocketWrapper::ShowButton, UI, &MainWindow::ShowButton);
        connect(Wrapper, &SocketWrapper::HideButton, UI, &MainWindow::HideButton);
        connect(Wrapper, &SocketWrapper::HideTextInput, UI, &MainWindow::HideTextInput);
        connect(Wrapper, &SocketWrapper::ShowTextInput, UI, &MainWindow::ShowTextInput);
        Socket->connectToHost(HOSTNAME, Port);
        if (!Socket->waitForConnected(10000)) {
            qDebug() << "Ket noi that bai";
        }
       
        Socket->write("HELLO ");
        exec();

    }
    void ReadMessage() {
        char Buffer[1024];
        qint64 Bytes = Socket->read(Buffer, 1024);
        QString S(Buffer);
        emit SendMessagetoGUI(S);



    }
    void ReceiveMessageFromEmail(const QString &S) {
        //emit IsProcessing();
        QByteArray Data = S.toUtf8();
        qDebug() << "END ";
        QString Request = getRequest(S);
        emit SendMessagetoGUI(S);
        QString L = "Request: " + Request;
        emit SendMessagetoGUI(L);
        HandleRequest(Socket, Request);

        
        //emit isFinished();

    }
    void onConnected() {
        qDebug() << "ket noi than cong!";
    }
    void onReadyRead() {

        QByteArray data = Socket->readAll();
        qDebug() << "Data from server:" << data;
    }

    void onError(QTcpSocket::SocketError socketError) {
        qDebug() << "Error: " << Socket->errorString();
    }
private slots:
    void SendMessage1() {
        QString Mess = UI->ChatPage->InputText->text();
        if (Mess != "") {
            qDebug() << Mess;
            QByteArray Data = Mess.toUtf8();
            QMetaObject::invokeMethod(Wrapper, "writeData", Qt::QueuedConnection, Q_ARG(QByteArray, Data));
           
            qDebug() << "HELLO ABC";
        }
    }

signals:
    void SendMessagetoGUI(const QString& S);
signals:
    void IsProcessing();
signals:
    void IsFinished();

   
public:
    ~ClientThread() {
        delete Socket;
        Socket = NULL;
    }
    friend class ClientApp;
};