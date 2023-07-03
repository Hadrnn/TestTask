// -*- coding: utf-8 -*-
#include "TextFS.h"
#include <iostream>
#include <fstream>
#include <exception>

TestTask::File* TestTask::textFS::Open(const char* name) {

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// блокируем открытие/создание/закрытие файлов

	std::string filePath(name); // string содержащий "фиктивный" путь к файлу

	if (TestTask::writeOnlyFiles.contains(filePath)) { // если открыт в writeOnly режиме - выходим
		std::cerr << "Didnt open a file in read only mode since it was already opened in write only mode\n";
		return nullptr;
	}

	std::filesystem::path directoryPath = std::filesystem::path(filePath).parent_path(); //путь к директории к файлу
	std::filesystem::path headerPath = directoryPath / headerFileName; // путь к header 
	std::string fileName = std::filesystem::path(filePath).filename().string(); // имя самого файла

	std::ifstream header;
	header.open(headerPath);
	if (!header.good() || !std::filesystem::exists(headerPath)) { // нет header - значит нет и файла (выходим); проблемы с ним - выходим
		header.close();
		std::cerr << "Couldnt find header while trying to open a file\n";
		return nullptr;
	}

	std::string buff;
	size_t filesPerPack = 0;
	size_t fileOffset = 0;

	try { // читаем информацию о VFS из header: максимальное количество файлов в одной пачке и максимальное количество символов на файл
		std::getline(header, buff);
		filesPerPack = stoi(buff);
		std::getline(header, buff);
		fileOffset = stoi(buff);
	}
	catch (const std::exception&) { // не смогли прочитать
		std::cerr << "Error while working with header\n";
		header.close();
		return nullptr;
	}

	size_t currFilePosition = 0;
	bool didntFind = true;

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff == fileName) {
			didntFind = false;
			break;
		}
		++currFilePosition;
	}

	if (didntFind) { // в header нет файла - значит его нет в VFS (выходим)
		header.close();
		std::cerr << "Didnt find a file in VFS\n";
		return nullptr;
	}

	size_t packNumber = currFilePosition / filesPerPack;
	currFilePosition %= filesPerPack;

	File* file = new File; // создаем File
	file->filePath = filePath;
	file->fileOffset = fileOffset;
	file->filePosition = currFilePosition;
	file->indicatorPosition = 0;
	file->packFileName = directoryPath / (std::to_string(packNumber) + packFileFormat);

	std::fstream testStream;
	testStream.open(file->packFileName, std::ios::in | std::ios::out | std::ios::ate);

	if (testStream.good()) { // для проверки открываем .dat файл (пачку)
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
		// если возникли какие-то ошибки с .dat файлом
		testStream.close();
		std::cerr << "Error while working with pack\n";
		delete file;
		return nullptr;
	}
}

TestTask::File* TestTask::textFS::Create(const char* name) {

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// блокируем открытие/создание/закрытие файлов

	std::string filePath(name); // string содержащий "фиктивный" путь к файлу

	if (TestTask::readOnlyFiles.contains(filePath)) {
		std::cerr << "Didnt open a file in write only mode since it is already opened in read only mode\n";
		return nullptr;
	};

	std::filesystem::path directoryPath = std::filesystem::path(filePath).parent_path(); //путь к директории к файлу
	std::filesystem::path headerPath = directoryPath / headerFileName; // путь к header 
	std::string fileName = std::filesystem::path(filePath).filename().string(); // имя самого файла

	std::ifstream header;

	if (!std::filesystem::exists(headerPath)) { // нет header-а - значит нет и VFS (инициализируем)
		std::filesystem::create_directories(directoryPath); // сначала создаем все директории по пути 

		std::ofstream serviceStream;
		serviceStream.open(headerPath);

		// записываем дефолтные значения и имя файла
		serviceStream << defaultFilesPerPack << std::endl;
		serviceStream << defaultFileOffset << std::endl;
		serviceStream << fileName << std::endl;

		serviceStream.close();
	}

	header.open(headerPath);

	std::string buff;
	size_t filesPerPack = 0;
	size_t fileOffset = 0;

	try { // читаем информацию о VFS из header 
		std::getline(header, buff);
		filesPerPack = stoi(buff);
		std::getline(header, buff);
		fileOffset = stoi(buff);
	}
	catch (const std::exception&) { // не смогли прочитать
		std::cerr << "Error while working with header\n";
		header.close();
		return nullptr;
	}

	size_t currFilePosition = 0;
	bool didntFind = true;

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff == fileName) {
			didntFind = false;
			break;
		}
		++currFilePosition;
	}

	header.close();

	if (didntFind) { // в header нет файла - значит его нет в VFS (записываем в header)
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

	if (!std::filesystem::exists(file->packFileName)) { // если нет пачки, то создаем
		std::ofstream serviceStream;
		serviceStream.open(file->packFileName);
		serviceStream.close();
	}

	std::fstream testStream;
	testStream.open(file->packFileName, std::ios::in | std::ios::out | std::ios::ate);

	if (testStream.good()) { // для проверки открываем .dat файл (пачку) 
		if (TestTask::writeOnlyFiles.contains(filePath)) {
			++TestTask::writeOnlyFiles[filePath];
		}
		else {
			TestTask::writeOnlyFiles.emplace(std::make_pair(filePath, 1));
		}

		TestTask::fileMutexes.emplace(std::make_pair(filePath, std::make_unique<std::mutex>()));// mutex не копируется, поэтому так
		testStream.close();
		return file;
	}
	else {
		// если возникли какие-то ошибки с .dat файлом
		testStream.close();
		std::cerr << "Error while working with pack\n";
		delete file;
		return nullptr;
	}
}

size_t TestTask::textFS::Read(File* f, char* buff, size_t len) {

	if (!f) { // комментарии излишни (а зачем тогда?... (-_-)) 
		return 0;
	}

	// если файл не открыт в readonly режиме - выходим 
	if (!TestTask::readOnlyFiles.contains(f->filePath)) {
		return 0;
	}

	std::ifstream inputStream;
	inputStream.open(f->packFileName);
	inputStream.seekg(f->fileOffset * f->filePosition + f->indicatorPosition); // ставим курсор на место, на котором закончили в прошлый раз

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

	// если файл не открыт в writeonly режиме - выходим
	if (!TestTask::writeOnlyFiles.contains(f->filePath)) {
		return 0;
	}

	std::lock_guard<std::mutex> lock(*(TestTask::fileMutexes[f->filePath].get()));
	// блокируем запись

	std::fstream outputStream(f->packFileName, std::ios::in | std::ios::out | std::ios::ate);
	outputStream.seekp(f->fileOffset * f->filePosition + f->indicatorPosition); // ставим курсор на место, на котором закончили в прошлый раз

	size_t maxLength = f->fileOffset - f->indicatorPosition; // считаем максимальную длину записи
	size_t textLength = maxLength >= len ? len : maxLength; // считаем фактическую длину записи

	outputStream.write(buff, textLength);
	outputStream.close();

	f->indicatorPosition += textLength;

	return textLength;
}

void TestTask::textFS::Close(File* f) {

	if (!f) { // логично
		return;
	}

	std::lock_guard<std::mutex> lock(TestTask::mapAccess);
	// блокируем открытие/создание/закрытие файлов

	// чистим map 
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
