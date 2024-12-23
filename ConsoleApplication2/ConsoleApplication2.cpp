// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "ClientApp.h"


int main(int argc, char* argv[])
{
   //QString S = "123456";
   // bool Ok = true;
   // qint32 N = S.toUInt(&Ok);
   // qDebug() << N;

   std::string ClientID = "612933153793-1teill41m3i7t1qkit497uh6q8nkkhmc.apps.googleusercontent.com";
    std::string ClientSecrect = "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";
   std::string refeshToken = "1//04sP2Z68i9I7NCgYIARAAGAQSNwF-L9IrSEFQu8CWlFVcdWXH-1quABuTo-wMgwguTzaD4B5Z5mmBqGbEoiWGtxSOxpDySLMbVs4";
    std::string ownEmail = "quanphanpq147@gmail.com";
    QApplication A(argc, argv);
    ClientApp* App = new ClientApp("127.0.0.1", 8080, ClientID, ClientSecrect, refeshToken, ownEmail);
    App->Run();
    A.exec();
    
    
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
