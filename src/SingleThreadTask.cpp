#include "SingleThreadTask.h"
#include "TextFS.h"
#include <iostream>

void singleThreadTest(char* toWrite) {
	TestTask::textFS text;

	TestTask::File* file1 = text.Create("C:/test1/test2/test3.txt"); // ��������� �� ������ 

	std::cout << std::endl;
	TestTask::File* file2 = text.Open("C:/test1/test2/test3.txt"); // ������� ������� �� ������ ��� �� ����
	char* readTo = new char[11];
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // �������, ������� ���������� ���������
	std::cout << std::endl;

	std::cout << "Put " << text.Write(file1, toWrite, 10) << " symbols in a file\n"; // ���������� � ���� 
	text.Close(file1); // ��������� ���� 
	std::cout << "Put " << text.Write(file1, toWrite, 10) << " symbols in a file\n"; // ������� ��������
	std::cout << std::endl;


	file2 = text.Open("C:/test1/test2/test3.txt"); // ��������� �� ������
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // ������ �� �����
	std::cout << readTo; // ���������
	std::cout << std::endl;

	file2 = text.Open("C:/test1/test2/test4.txt"); // ��������� �� ������ ���� �������� ��� � VFS
	std::cout << "Read " << text.Read(file2, readTo, 10) << " symbols from a file\n"; // ������ �� �����
	std::cout << std::endl;

	TestTask::File* file3 = text.Open("C:/test1/test2/test3.txt"); // ��������� ��� �� ���� �� ������
	std::cout << "Read " << text.Read(file3, readTo, 10) << " symbols from a file\n"; // ������ 
	std::cout << readTo; // ���������
	std::cout << "Put " << text.Write(file3, toWrite, 10) << " symbols in a file\n"; // �������� �������� � ���� 
	std::cout << std::endl;

	text.Close(file2);
	text.Close(file3);
}
