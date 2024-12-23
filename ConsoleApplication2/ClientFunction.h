#pragma once
#include <QtNetwork/QTcpSocket>
#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>

struct ServiceInfo {
    QString serviceName;
    QString displayName;
    uint32_t serviceStatus;
};
inline QString ServiceStatusToString(uint32_t status) {
    switch (status) {
    case 1: return QString("Stopped");
    case 2: return QString("Start Pending");
    case 3: return QString("Stop Pending");
    case 4: return QString("Running");
    case 5: return QString("Continue Pending");
    case 6: return QString("Pause Pending");
    case 7: return QString("Paused");
    default: return QString("Unknown");
    }
}
inline void receiveFile(QTcpSocket* socket, const QString& filename) {
    if (!socket || !socket->isOpen()) {
        qWarning() << "Socket is not valid or not open.";
        return;
    }

    // 
    int j = 0;
    uint32_t fileSize = 0;
    QByteArray sizeBuffer;
    while (sizeBuffer.size() < static_cast<int>(sizeof(uint32_t))) {
        if (!socket->waitForReadyRead(30000)) {  // Timeout 3s
            qWarning() << "Timeout while waiting for file size.";
            return;
        }
        j++;
        std::cout << j << std::endl;
        sizeBuffer.append(socket->read(static_cast<int>(sizeof(uint32_t)) - sizeBuffer.size()));
    }
    QDataStream sizeStream(sizeBuffer);
    sizeStream >> fileSize;

    std::cout << "Receiving file of size:" << fileSize << "bytes";

    // Tao buffer va mo file de ghi
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << file.errorString();
        return;
    }

    QByteArray fileBuffer;
    quint64 totalBytesReceived = 0;
    int i = 0;
    while (totalBytesReceived < fileSize) {
        socket->waitForReadyRead();
        i++;
        std::cout <<"i = "<< i << std::endl;
        // Doc tung phan du lieu , toi da 1024 byte 
        QByteArray chunk = socket->read(qMin(static_cast<qint64>(2048), static_cast<qint64>(fileSize - totalBytesReceived)));
        file.write(chunk);  // 
        totalBytesReceived += chunk.size();

        std::cout << "Received:" << totalBytesReceived << "/" << fileSize << "bytes";
    }

    file.close();
    qDebug() << "File received successfully and saved as:" << filename;
}
