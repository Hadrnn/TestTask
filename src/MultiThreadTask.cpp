// -*- coding: utf-8 -*-
#include "MultiThreadTask.h"
#include "TextFS.h"
#include <iostream>

void multiThreadTest() {
	// два из трех потоков будут работать с одной пачкой
	char thread1FileName[] = "C:/test1/test2/test5.txt\0";
	char thread2FileName[] = "C:/test1/test2/test6.txt\0";
	char thread3FileName[] = "C:/test1/test2/test7.txt\0";

	std::thread t1(threadTask1, 'a', 1, thread1FileName);
	std::thread t2(threadTask1, 'b', 2, thread2FileName);
	std::thread t3(threadTask1, 'c', 3, thread3FileName);

	t1.join();
	t2.join();
	t3.join();
	std::cout << std::endl;

	std::thread t4(threadTask2, 4, thread1FileName);
	std::thread t5(threadTask2, 5, thread2FileName);
	std::thread t6(threadTask2, 6, thread3FileName);


	t4.join();
	t5.join();
	t6.join();
}

void threadTask1(char toWrite, int threadNumber, char* filePath) {
	TestTask::textFS text;

	TestTask::File* myFile = text.Create(filePath);

	for (int i = 0; i < 15; ++i) {
		if (text.Write(myFile, &toWrite, 1) == 1) {
			std::cout << "Thread " + std::to_string(threadNumber) + " put a symbol\n"; // тк defaultFileOffset = 10 мы сможем записать только 10 символов
		}
		else {
			std::cout << "Thread " + std::to_string(threadNumber) + " couldnt put a symbol\n"; // и получим 5 таких сообщений на каждый поток
		}
	}

	text.Close(myFile);
}

void threadTask2(int threadNumber, char* filePath) {
	TestTask::textFS text;

	TestTask::File* myFile = text.Open(filePath);
	char* toRead = new char[2];

	for (int i = 0; i < 15; ++i) {
		int x = 0;
		if (text.Read(myFile, toRead, 1) == 1) {
			std::cout << "Thread " + std::to_string(threadNumber) + " read a symbol: " + toRead + '\n'; // тк defaultFileOffset = 10 мы сможем прочитать только 10 символов
		}
		else {
			std::cout << "Thread " + std::to_string(threadNumber) + " couldnt read a symbol\n"; // и получим 5 таких сообщений на каждый поток
		}
	}

	text.Close(myFile);
}
