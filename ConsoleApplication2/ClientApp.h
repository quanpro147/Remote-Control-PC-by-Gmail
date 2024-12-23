#pragma once
#include"Client.h"
#include"GmailClient.h"
class ClientApp:public QObject{
	Q_OBJECT
private:
	MainWindow* UI;
	ClientThread* Threading;
	GmailClient* MAIL;
public:
	ClientApp(const QString Hostname, qint16 PORT, const std::string& clientId, const std::string& clientSecret,
		const std::string& refreshToken, const std::string& ownEmail) {
		UI = new MainWindow();
		Threading = new ClientThread(Hostname, PORT, UI);
		MAIL = new GmailClient(clientId, clientSecret, refreshToken, ownEmail);
		QObject::connect(MAIL, &GmailClient::SendMessagetoSocket, Threading, &ClientThread::ReceiveMessageFromEmail);
		QObject::connect(Threading, &ClientThread::IsProcessing, MAIL, &GmailClient::IsProcessing);
		QObject::connect(Threading, &ClientThread::IsFinished, MAIL, &GmailClient::FinishProcessing);
		QObject::connect(UI->ChatPage->SendButton, &QPushButton::clicked, Threading, &ClientThread::SendMessage1);


	}
	void Run() {
		Threading->start();
		MAIL->start();
		UI->show();
	}
	~ClientApp() {
		if (UI != NULL) {
			delete UI;
		}
		if (Threading != NULL || Threading->isRunning()) {
			Threading->quit();
			delete Threading;
		}
		if (MAIL != NULL || MAIL->isRunning()) {
			MAIL->quit();
			delete MAIL;
		}
	}
};