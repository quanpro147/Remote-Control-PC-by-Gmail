#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include<QtCore/qstring.h>

class ChatWidget :public QWidget {
	Q_OBJECT
private:
	QPlainTextEdit* PlainText = NULL;
	QPushButton* BackButton = NULL;
	QPushButton* HelpButton = NULL;
	QPushButton* SendButton = NULL;
	QLineEdit* InputText = NULL;
	QPushButton* SendNumber = NULL;
	QLineEdit* InputNumber = NULL;
	void SetText() {
		QString a = "Help ?";
		QString b = "Back";
		HelpButton->setText(a);
		BackButton->setText(b);
	}
	void Update(const QString& S) {
		PlainText->appendPlainText(S);
	}
	void UpdateText() {
		QString S = InputText->text();
		if (S != "") {
			Update(S);
		}
	}
public:
	ChatWidget() {

		setGeometry(QRect(0, 0, 1000, 800));

		PlainText = new QPlainTextEdit(this);
		BackButton = new QPushButton(this);
		HelpButton = new QPushButton(this);
		SendButton = new QPushButton(this);
		InputText = new QLineEdit(this);
		SendNumber = new QPushButton(this);
		InputNumber = new QLineEdit(this);
		PlainText->setGeometry(QRect(300, 100, 600, 500));
		HelpButton->setGeometry(QRect(160, 100, 120, 40));
		BackButton->setGeometry(QRect(160, 560, 120, 40));
		InputText->setGeometry(QRect(520, 620, 160, 40));
		SendButton->setGeometry(QRect(420, 620, 100, 40));
		InputNumber->setGeometry(QRect(520, 620, 160, 40));
		SendNumber->setGeometry(QRect(420, 620, 100, 40));
		PlainText->setReadOnly(true);
		SetText();
		connect(SendButton, &QPushButton::clicked, this, &ChatWidget::UpdateText);
		SendButton->hide();
		InputText->hide();
		SendNumber->hide();
		InputNumber->hide();




	}
	friend class MainWindow;
	friend class ClientApp;
	friend class ClientThread;
};
