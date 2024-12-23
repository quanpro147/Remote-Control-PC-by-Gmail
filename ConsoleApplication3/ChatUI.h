#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include<QtGui/qvalidator.h>
#include<QtCore/qstring.h>
class ChatWidget :public QWidget {
private:
	QPlainTextEdit* PlainText = NULL;
	QPushButton* BackButton = NULL;
	QPushButton* HelpButton = NULL;
	QPushButton* SendButton = NULL;
	QPushButton* StopOrStart = NULL;
	bool Start = true;
	QLineEdit* InputText = NULL;
	QIntValidator* IntValid = NULL;
	bool CanInput = true;
	void SetText() {
		QString a = "Help111 ?";
		QString b = "Back";
		HelpButton->setText(a);
		BackButton->setText(b);
	}
	void Update(const QString& S) {
		PlainText->appendPlainText(S);
	}
	void Change() {
		if (CanInput) {
			CanInput = false;
			InputText->hide();
		}
		else {
			CanInput = true;
			InputText->show();
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
		IntValid = new QIntValidator(0, 1000, this);
		PlainText->setGeometry(QRect(300, 100, 600, 500));
		HelpButton->setGeometry(QRect(160, 100, 120, 40));
		BackButton->setGeometry(QRect(160, 560, 120, 40));
		InputText->setGeometry(QRect(520, 620, 160, 40));
		SendButton->setGeometry(QRect(420, 620, 100, 40));
		InputText->setValidator(IntValid);
		SetText();
		//connect(SendButton, &QPushButton::clicked, this, &ChatWidget::Change);





	}
	friend class MainWindow;

};
