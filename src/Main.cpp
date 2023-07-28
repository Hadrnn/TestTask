#include <iostream>
#include "TextFS.h"


int main() {

	TestTask::textFS filesys;
	TestTask::File* myfile = filesys.Create("C:/test1/test2/test3.txt");
	char message[] = "Hi there ! My name is Timur ";
	char message1[] = "Whats up? ";
	char message2[] = " Hello! Very nice to meet you! ";

	filesys.Write(myfile, message, 10);

	TestTask::File* myfile2 = filesys.Open("C:/test1/test2/test3.txt"); // не откроется, потому что открыли в другом режиме 

	TestTask::File* myfile3 = filesys.Create("C:/test1/test2/test4.txt");
	filesys.Write(myfile3, message1, 10); // забиваем второй кластер этим файлом 
	filesys.Write(myfile, message2, 30); // пытаемся дописать в первый

	filesys.Close(myfile);
	filesys.Close(myfile2);
	filesys.Close(myfile3);

	TestTask::File* myfile4 = filesys.Open("C:/test1/test2/test3.txt");
	char readMessage[] = "                                        ";
	filesys.Read(myfile4, readMessage, 40);
	std::cout << readMessage;
	filesys.Close(myfile4);

	return 0;
}

