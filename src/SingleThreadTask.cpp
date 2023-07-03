#include "SingleThreadTask.h"
#include "TextFS.h"
#include <iostream>

void singleThreadTest(char* toWrite) {
	TestTask::textFS text;

	TestTask::File* file1 = text.Create("C:/test1/test2/test3.txt"); // открываем на запись 

	std::cout << std::endl;
	TestTask::File* file2 = text.Open("C:/test1/test2/test3.txt"); // пробуем открыть на чтение тот же файл
	char* readTo = new char[11];
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // смотрим, сколько получилось прочитать
	std::cout << std::endl;

	std::cout << "Put " << text.Write(file1, toWrite, 10) << " symbols in a file\n"; // записываем в файл 
	text.Close(file1); // закрываем файл 
	std::cout << "Put " << text.Write(file1, toWrite, 10) << " symbols in a file\n"; // пробуем записать
	std::cout << std::endl;


	file2 = text.Open("C:/test1/test2/test3.txt"); // открываем на чтение
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // читаем из файла
	std::cout << readTo; // результат
	std::cout << std::endl;

	file2 = text.Open("C:/test1/test2/test4.txt"); // открываем на чтение файл которого нет в VFS
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // читаем из файла
	std::cout << std::endl;

	TestTask::File* file3 = text.Open("C:/test1/test2/test3.txt"); // открываем тот же файл на чтение
	std::cout << "Read " << text.Read(file3, readTo, 10) << " symbols from a file\n"; // читаем 
	std::cout << readTo; // результат
	std::cout << "Put " << text.Write(file3, toWrite, 10) << " symbols in a file\n"; // пытаемся записать в файл 
	std::cout << std::endl;

	text.Close(file2);
	text.Close(file3);

	delete file1;
	delete file2;
	delete file3;
	delete readTo;
}
