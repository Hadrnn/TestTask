// -*- coding: utf-8 -*-
#include "TextFS.h"
#include <iostream>
#include <fstream>
#include <exception>

TestTask::File* TestTask::textFS::Open(const char* name) {

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// ��������� ��������/��������/�������� ������

	std::string filePath(name); // string ���������� "���������" ���� � �����

	if (TestTask::writeOnlyFiles.contains(filePath)) { // ���� ������ � writeOnly ������ - �������
		std::cerr << "Didnt open a file in read only mode since it was already opened in write only mode\n";
		return nullptr;
	}

	std::filesystem::path directoryPath = std::filesystem::path(filePath).parent_path(); //���� � ���������� � �����
	std::filesystem::path headerPath = directoryPath / headerFileName; // ���� � header 
	std::string fileName = std::filesystem::path(filePath).filename().string(); // ��� ������ �����

	std::ifstream header;
	header.open(headerPath);
	if (!header.good() || !std::filesystem::exists(headerPath)) { // ��� header - ������ ��� � ����� (�������); �������� � ��� - �������
		header.close();
		std::cerr << "Couldnt find header while trying to open a file\n";
		return nullptr;
	}

	std::string buff;
	size_t filesPerPack = 0;
	size_t fileOffset = 0;

	try { // ������ ���������� � VFS �� header: ������������ ���������� ������ � ����� ����� � ������������ ���������� �������� �� ����
		std::getline(header, buff);
		filesPerPack = stoi(buff);
		std::getline(header, buff);
		fileOffset = stoi(buff);
	}
	catch (const std::exception&) { // �� ������ ���������
		std::cerr << "Error while working with header\n";
		header.close();
		return nullptr;
	}

	size_t currFilePosition = 0;
	bool didntFind = true;

	while (std::getline(header, buff)) { // ���� ������ ���� � header
		if (buff == fileName) {
			didntFind = false;
			break;
		}
		++currFilePosition;
	}

	if (didntFind) { // � header ��� ����� - ������ ��� ��� � VFS (�������)
		header.close();
		std::cerr << "Didnt find a file in VFS\n";
		return nullptr;
	}

	size_t packNumber = currFilePosition / filesPerPack;
	currFilePosition %= filesPerPack;

	File* file = new File; // ������� File
	file->filePath = filePath;
	file->fileOffset = fileOffset;
	file->filePosition = currFilePosition;
	file->indicatorPosition = 0;
	file->packFileName = directoryPath / (std::to_string(packNumber) + packFileFormat);

	std::fstream testStream;
	testStream.open(file->packFileName, std::ios::in | std::ios::out | std::ios::ate);

	if (testStream.good()) { // ��� �������� ��������� .dat ���� (�����)
		if (TestTask::readOnlyFiles.contains(filePath)) {
			++TestTask::readOnlyFiles[filePath];
		}
		else {
			TestTask::readOnlyFiles.emplace(std::make_pair(filePath, 1));
		}
		testStream.close();
		return file;
	}
	else {
		// ���� �������� �����-�� ������ � .dat ������
		testStream.close();
		std::cerr << "Error while working with pack\n";
		delete file;
		return nullptr;
	}
}

TestTask::File* TestTask::textFS::Create(const char* name) {

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// ��������� ��������/��������/�������� ������

	std::string filePath(name); // string ���������� "���������" ���� � �����

	if (TestTask::readOnlyFiles.contains(filePath)) {
		std::cerr << "Didnt open a file in write only mode since it is already opened in read only mode\n";
		return nullptr;
	};

	std::filesystem::path directoryPath = std::filesystem::path(filePath).parent_path(); //���� � ���������� � �����
	std::filesystem::path headerPath = directoryPath / headerFileName; // ���� � header 
	std::string fileName = std::filesystem::path(filePath).filename().string(); // ��� ������ �����

	std::ifstream header;

	if (!std::filesystem::exists(headerPath)) { // ��� header-� - ������ ��� � VFS (��������������)
		std::filesystem::create_directories(directoryPath); // ������� ������� ��� ���������� �� ���� 

		std::ofstream serviceStream;
		serviceStream.open(headerPath);

		// ���������� ��������� �������� � ��� �����
		serviceStream << defaultFilesPerPack << std::endl;
		serviceStream << defaultFileOffset << std::endl;
		serviceStream << fileName << std::endl;

		serviceStream.close();
	}

	header.open(headerPath);

	std::string buff;
	size_t filesPerPack = 0;
	size_t fileOffset = 0;

	try { // ������ ���������� � VFS �� header 
		std::getline(header, buff);
		filesPerPack = stoi(buff);
		std::getline(header, buff);
		fileOffset = stoi(buff);
	}
	catch (const std::exception&) { // �� ������ ���������
		std::cerr << "Error while working with header\n";
		header.close();
		return nullptr;
	}

	size_t currFilePosition = 0;
	bool didntFind = true;

	while (std::getline(header, buff)) { // ���� ������ ���� � header
		if (buff == fileName) {
			didntFind = false;
			break;
		}
		++currFilePosition;
	}

	header.close();

	if (didntFind) { // � header ��� ����� - ������ ��� ��� � VFS (���������� � header)
		std::ofstream serviceStream;
		serviceStream.open(headerPath, std::ios::app);
		serviceStream << fileName << std::endl;
		serviceStream.close();
	}

	size_t packNumber = currFilePosition / filesPerPack;
	currFilePosition %= filesPerPack;

	File* file = new File;
	file->filePath = filePath;
	file->fileOffset = fileOffset;
	file->filePosition = currFilePosition;
	file->indicatorPosition = 0;
	file->packFileName = directoryPath / (std::to_string(packNumber) + packFileFormat);

	if (!std::filesystem::exists(file->packFileName)) { // ���� ��� �����, �� �������
		std::ofstream serviceStream;
		serviceStream.open(file->packFileName);
		serviceStream.close();
	}

	std::fstream testStream;
	testStream.open(file->packFileName, std::ios::in | std::ios::out | std::ios::ate);

	if (testStream.good()) { // ��� �������� ��������� .dat ���� (�����) 
		if (TestTask::writeOnlyFiles.contains(filePath)) {
			++TestTask::writeOnlyFiles[filePath];
		}
		else {
			TestTask::writeOnlyFiles.emplace(std::make_pair(filePath, 1));
		}

		TestTask::fileMutexes.emplace(std::make_pair(filePath, std::make_unique<std::mutex>()));// mutex �� ����������, ������� ���
		testStream.close();
		return file;
	}
	else {
		// ���� �������� �����-�� ������ � .dat ������
		testStream.close();
		std::cerr << "Error while working with pack\n";
		delete file;
		return nullptr;
	}
}

size_t TestTask::textFS::Read(File* f, char* buff, size_t len) {

	if (!f) { // ����������� ������� (� ����� �����?... (-_-)) 
		return 0;
	}

	// ���� ���� �� ������ � readonly ������ - ������� 
	if (!TestTask::readOnlyFiles.contains(f->filePath)) {
		return 0;
	}

	std::ifstream inputStream;
	inputStream.open(f->packFileName);
	inputStream.seekg(f->fileOffset * f->filePosition + f->indicatorPosition); // ������ ������ �� �����, �� ������� ��������� � ������� ���

	size_t initialPosition = f->indicatorPosition;
	size_t i = 0;

	for (; i < len && f->indicatorPosition < f->fileOffset && inputStream.good(); ++i, ++f->indicatorPosition) {
		inputStream.get(buff[i]);
	}
	buff[i] = '\0';

	return f->indicatorPosition - initialPosition;
}

size_t TestTask::textFS::Write(File* f, char* buff, size_t len) {

	if (!f) { // (-_-)
		return 0;
	}

	// ���� ���� �� ������ � writeonly ������ - �������
	if (!TestTask::writeOnlyFiles.contains(f->filePath)) {
		return 0;
	}

	std::lock_guard<std::mutex> lock(*(TestTask::fileMutexes[f->filePath].get()));
	// ��������� ������

	std::fstream outputStream(f->packFileName, std::ios::in | std::ios::out | std::ios::ate);
	outputStream.seekp(f->fileOffset * f->filePosition + f->indicatorPosition); // ������ ������ �� �����, �� ������� ��������� � ������� ���

	size_t maxLength = f->fileOffset - f->indicatorPosition; // ������� ������������ ����� ������
	size_t textLength = maxLength >= len ? len : maxLength; // ������� ����������� ����� ������

	outputStream.write(buff, textLength);
	outputStream.close();

	f->indicatorPosition += textLength;

	return textLength;
}

void TestTask::textFS::Close(File* f) {

	if (!f) { // �������
		return;
	}

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// ��������� ��������/��������/�������� ������

	// ������ map 
	if (TestTask::readOnlyFiles.contains(f->filePath)) {
		if (TestTask::readOnlyFiles[f->filePath] <= 1) {
			TestTask::readOnlyFiles.erase(f->filePath);
		}
		else {
			--TestTask::readOnlyFiles[f->filePath];
		}
	}

	if (TestTask::writeOnlyFiles.contains(f->filePath)) {
		if (TestTask::writeOnlyFiles[f->filePath] <= 1) {
			TestTask::writeOnlyFiles.erase(f->filePath);
			TestTask::fileMutexes.erase(f->filePath);
		}
		else {
			--TestTask::writeOnlyFiles[f->filePath];
		}
	}

};
