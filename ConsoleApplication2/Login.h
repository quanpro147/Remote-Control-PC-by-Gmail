#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
class LoginWidget :public QWidget {
	Q_OBJECT
private:
	QLineEdit* Username_text = NULL;
	QLineEdit* Password_text = NULL;
	QLabel* User_Name = NULL;
	QLabel* Password = NULL;
	QPushButton* LoginButton = NULL;
	void SetText() {
		QString a = "Username1111";
		QString b = "Password";
		QString c = "Login";
		User_Name->setText(a);
		Password->setText(b);
		LoginButton->setText(c);
	}
	LoginWidget() {

		setGeometry(QRect(0, 0, 1000, 800));
		LoginButton = new QPushButton(this);
		Username_text = new QLineEdit(this);
		Password_text = new QLineEdit(this);
		User_Name = new QLabel(this);
		Password = new QLabel(this);
		//Setup Username_text
		Username_text->setGeometry(QRect(420, 200, 160, 40));
		Username_text->setStyleSheet(QString::fromUtf8("border-radius:5px  ;border:2px solid black;"));



		//Setup Password_text
		Password_text->setGeometry(QRect(420, 260, 160, 40));
		Password_text->setStyleSheet(QString::fromUtf8("border-radius:5px  ;border:2px solid black;"));


		//setup User_name
		User_Name->setGeometry(QRect(320, 200, 90, 40));
		User_Name->setStyleSheet(QString::fromUtf8("border-radius:5px  ;border:2px solid black;"));
		Username_text->setStyleSheet(QString::fromUtf8("border:2px solid black;\n"
			"text-align: justify;\n"
			"left:2px"));



		//Setup Password
		Password->setGeometry(QRect(320, 260, 90, 40));
		Password->setStyleSheet(QString::fromUtf8("border-radius:5px  ;border:2px solid black;"));


		LoginButton->setGeometry(QRect(440, 320, 120, 40));
		LoginButton->setStyleSheet(QString::fromUtf8("border-radius:5px  ;border:2px solid black;"));
		SetText();



	}
	friend class MainWindow;

};