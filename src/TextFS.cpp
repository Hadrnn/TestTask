﻿#include "TextFS.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <exception>

struct VFSInfo { // структурка, в которую будем записывать данные о VFS
	operator bool() { return clusterSize > 0 && FirstEmptyCluster >= 0; }
	int clusterSize = -1;
	int FirstEmptyCluster = -1;
};

struct FileInfo {
	std::string fileName;
	int firstCluster = -1;
	std::string mode;
	int numberOfThreads = 0;

	FileInfo(const std::string fileName_) : fileName(fileName_) {};

	FileInfo(const std::string fileName_, int firstCluster_, std::string mode_, int numberOfThreads) : 
		fileName(fileName_) , firstCluster(firstCluster_), mode(mode_),numberOfThreads(numberOfThreads) {};

};

std::ostream& operator<<(std::ostream& os, const FileInfo& info) {
	os << info.fileName << " ";
	os << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << info.firstCluster;
	os << ' ' << info.mode + std::string(TestTask::maxModeMarkLength - info.mode.length(), ' ');
	os << ' ' << std::setw(TestTask::maxThreadsCounterLength) << std::setfill('0') << info.numberOfThreads;
	os << '\n';
	return os;
}

std::istream& operator>>(std::istream& is, FileInfo& info) {

	std::string buff;
	std::getline(is, buff);

	buff = buff.substr(info.fileName.length() + 1, buff.length());

	try {
		info.firstCluster = std::stoi(buff);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		info.firstCluster = TestTask::faultyCluster;
	}

	buff = buff.substr(TestTask::maxClusterDigits + 1, buff.length());
	info.mode = buff.substr(0, TestTask::maxModeMarkLength);

	try {
		info.numberOfThreads = std::stoi(buff.substr(TestTask::maxModeMarkLength, buff.length()));
		// будет равен 0 только если файл не открыт в каком-либо режиме (нет метки режима или количество потоков = 0)
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		info.numberOfThreads = 0;
	}
	return is;
}

std::filesystem::path VFSInit(std::string filePath) { // инициализыция

	//std::lock_guard(TestTask::VFSCritical);// блокикуем VFS

	std::filesystem::path VFSPath(filePath);

	while (!std::filesystem::exists(VFSPath) && VFSPath != VFSPath.root_path()) { // ищем существющую папку из filePath (первой найдется та, что "глубже" лежит)
		VFSPath = VFSPath.parent_path();
	}
	
	std::ofstream serviceStream;     // создаем три файла, которые необходимы для работы VFS
	serviceStream.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::binary);  // в Header записываем данные о VFS
	serviceStream << TestTask::clusterSizeMark + std::string(TestTask::maxSettingLength - TestTask::clusterSizeMark.length(), ' ');
	serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << TestTask::defaultClusterSize << '\n';
	serviceStream << TestTask::firstEmptyClusterMark + std::string(TestTask::maxSettingLength - TestTask::firstEmptyClusterMark.length(), ' ');
	serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << 0 << '\n';
	serviceStream << TestTask::endOfVFSInfo << '\n';
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::binary); // в Table записываем данные о первом кластере (он пустой)
	serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSDataFileName, std::ios::binary); // Data файл остается пустым
	serviceStream.close();

	return VFSPath;
}

std::filesystem::path findVFSPath(std::string filePath) {

	std::filesystem::path VFSPath(filePath);

	if (VFSPath.empty())
		throw std::runtime_error("Empty path to VFS\n");

	while (VFSPath != VFSPath.root_path()) { // в filePath ищем папку, в которой инициализирована VFS
		VFSPath = VFSPath.parent_path();
		if (std::filesystem::exists(VFSPath / TestTask::VFSHeaderFileName) &&
			std::filesystem::exists(VFSPath / TestTask::VFSTableFileName) && 
			std::filesystem::exists(VFSPath / TestTask::VFSDataFileName)) {

			std::ifstream serviceStream;  // проверяем целостность файлов
			
			serviceStream.open(VFSPath / TestTask::VFSHeaderFileName);
			if(!serviceStream.good())
				throw std::runtime_error("Could not open VFS Header\n");
			serviceStream.close();

			serviceStream.open(VFSPath / TestTask::VFSTableFileName);
			if (!serviceStream.good())
				throw std::runtime_error("Could not open VFS Table\n");
			serviceStream.close();

			serviceStream.open(VFSPath / TestTask::VFSDataFileName);
			if (!serviceStream.good())
				throw std::runtime_error("Could not open VFS Data file\n");
			serviceStream.close();

			return VFSPath;
		}
	};

	if (VFSPath == VFSPath.root_path()) {
		VFSPath = TestTask::errorPath;
	}

	return VFSPath;
}

VFSInfo getVFSInfo(std::filesystem::path VFSPath) { // получаем информацию о VFS из Header

	std::ifstream header;
	std::string buff;

	VFSInfo info;

	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::binary);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header\n");
	}
	
	std::getline(header, buff);
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos && !header.eof()) { // Параметры VFS могут лежать в Header в любом порядке

		if (buff.find(TestTask::clusterSizeMark) != std::string::npos) {
			try {
				info.clusterSize = std::stoi(buff.substr(TestTask::maxSettingLength, buff.length()));
			}
			catch (const std::exception&) {
				std::cerr << "Error while working with VFS Header\n";
			}
		}
		else if (buff.find(TestTask::firstEmptyClusterMark) != std::string::npos) {
			try {
				info.FirstEmptyCluster = std::stoi(buff.substr(TestTask::maxSettingLength,buff.length()));
			}
			catch (const std::exception&) {
				std::cerr << "Error while working with VFS Header\n";
			}
		}
		std::getline(header, buff);
	}

	header.close();
	return info;
}

int getFileCluster(std::filesystem::path VFSPath, std::string fileName,std::string mode) { // ищем изначальный кластер файла
	
	std::string buff;
	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::binary);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header\n");
	}

	int pointerPos = 0;
	FileInfo info(fileName);

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) { // нашли

			
			header.seekg(pointerPos, std::ios_base::beg);
			header >> info;

			if (info.numberOfThreads && buff.find(mode) == std::string::npos) { // если с данным файлом работает хоть один поток в другом режиме
				throw  std::runtime_error("File was already opened in opposing to " + mode + " mode\n");
			}
			else { // либо совпал режим, либо количество рабочих потоков -  ноль
				if (++info.numberOfThreads >TestTask::maxThreadsCount) {
					throw  std::runtime_error("Too many threads for one file\n");
				}
				header.clear();
				header.seekp(pointerPos, std::ios_base::beg);
				header << info; // перед этим увеличилии количество рабочих потоков (см. несколько строк выше)
				return info.firstCluster;
			}
		}
		pointerPos = header.tellp();
	}

	header.close();

	return TestTask::didNotFindCluster; // в том случае, если не нашли файл в VFSHeader
}

void deleteModeMark(std::filesystem::path VFSPath, std::string fileName) {

	std::string buff;
	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::binary);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header\n");
	}

	int pointerPos = 0;
	FileInfo info(fileName);

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) {

			header.seekg(pointerPos, std::ios_base::beg);
			header >> info;

			--info.numberOfThreads;

			header.clear();
			header.seekp(pointerPos, std::ios_base::beg);
			header << info;
			header.close();
			return;
		}
		pointerPos = header.tellp();
	}
	header.close();
}

int findEmptyCluster(std::filesystem::path VFSPath, int from = 0) { // поиск самого ближнего к началу Data фаqла свободного кластера 
	std::fstream serviceStream;

	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out | std::ios::binary);
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table\n");
	}
	
	int clusterStatus = 0;
	int currentLine = 0;
	std::string buff;

	while (!serviceStream.eof()) {
		
		serviceStream >> clusterStatus;

		if (currentLine > from) {
				if (clusterStatus == TestTask::clusterIsEmpty)
				return currentLine;
		}
		++currentLine;
	}
	
	//std::lock_guard(TestTask::VFSCritical);
	serviceStream.seekp(0, std::ios_base::end);
	serviceStream.clear();
	serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';

	return currentLine;
}

void refreshVFSHeader(std::filesystem::path VFSPath, VFSInfo info) {
	// обновляем информацию в Header (пока что обновляется только позиция первого свободного кластера)
	// с расширением возможностей VFS можно обновлять и другие данные

	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::binary);
	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header\n");
	}
	
	std::string buff;
	size_t lineNumber = 0;
	std::getline(header, buff);
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos && !header.eof()) {

		 if (buff.find(TestTask::firstEmptyClusterMark) != std::string::npos) { // работает - не чини
			header.seekp(lineNumber * (TestTask::maxSettingLength + TestTask::maxClusterDigits + 2), std::ios::beg);
			header << TestTask::firstEmptyClusterMark + std::string(TestTask::maxSettingLength - TestTask::firstEmptyClusterMark.length(), ' ');
			header << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << info.FirstEmptyCluster;
			header << '\n';
			break;
		}
		std::getline(header, buff);
		++lineNumber;
 	}
	header.close();
}

void changeClusterAssigment(std::filesystem::path VFSPath, int clusterNumber, int changeTo) { // переназначаем ссылку на следующий кластер
	std::fstream serviceStream;
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out | std::ios::binary);
	
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table\n");
	}

	int currentCluster = 0;
	
	while (!serviceStream.eof()) {
		
		int currentAssigment = 0;
		serviceStream >> currentAssigment;

		if (currentCluster == clusterNumber) {
			serviceStream.seekp((TestTask::maxClusterDigits + 2)*currentCluster, std::ios::beg);
			serviceStream.clear();

			if (changeTo >= 0) {
				serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << changeTo << '\n';
			}
			else {
				serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(changeTo) << '\n';
			}
			break;
		}
		++currentCluster;
	}
	serviceStream.close();

	if (currentCluster < clusterNumber) {
		throw  std::runtime_error("Error while changing cluster assigment\n");
	}
}

int findNextCluster(std::filesystem::path VFSPath, int cluster) { // переходим по ссылку на следующий кластер 
	std::fstream serviceStream;
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out | std::ios::binary);
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table\n");
	}

	int clusterAssigment = 0;
	int currentLine = 0;

	while (!serviceStream.eof()) {

		serviceStream >> clusterAssigment;

		if (currentLine == cluster) {
				return clusterAssigment;
		}
		++currentLine;
	}

	serviceStream.close();

	return TestTask::didNotFindCluster;
}

int addFileToVFS(std::filesystem::path VFSPath, std::string fileName, std::string mode) { // добавляем файл в VFS

	try {
		//std::lock_guard(TestTask::VFSCritical);

		VFSInfo info = getVFSInfo(VFSPath);

		int currentEmptyCluster = info.FirstEmptyCluster;
		int nextEmptyCluster = findEmptyCluster(VFSPath, info.FirstEmptyCluster);
		info.FirstEmptyCluster = nextEmptyCluster;

		refreshVFSHeader(VFSPath, info);
		changeClusterAssigment(VFSPath, currentEmptyCluster, TestTask::endOfFile);
		changeClusterAssigment(VFSPath, nextEmptyCluster, TestTask::clusterIsEmpty);

		std::fstream header;
		header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::ate | std::ios::binary);
		if (!header.good()) {
			header.close();
			throw  std::runtime_error("Couldnt open VFS Header\n");
		}

		header.seekp(0, std::ios_base::end);
		FileInfo fileInfo(fileName, currentEmptyCluster, mode, 1);
		
		header << fileInfo;
		header.close();

		return currentEmptyCluster;
	}
	catch (const std::exception&) {
		throw;
	}
}

TestTask::File* TestTask::textFS::Open(const char* name) {

	std::string filePath(name); 
	std::filesystem::path VFSPath;
	
	try {
		VFSPath = findVFSPath(filePath);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		return nullptr;
	}

	if (VFSPath == TestTask::errorPath) {
		return nullptr;
	}

	VFSInfo info = getVFSInfo(VFSPath);

	if (!info) {
		return nullptr;
	}

	File* file = nullptr;

	try {

		int fileCluster = getFileCluster(VFSPath, filePath, ReadOnlyMark);
		if (fileCluster == TestTask::didNotFindCluster ) {
			return nullptr;
		}

		file = new File(VFSPath, filePath, fileCluster, info.clusterSize, FileStatus::ReadOnly);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		delete file;
		return nullptr;
	}
	
	return file;
}

TestTask::File* TestTask::textFS::Create(const char* name) {

	std::string filePath(name);
	std::filesystem::path VFSPath;

	try {
		VFSPath = findVFSPath(filePath);
	}
	catch (const std::exception& e) { // другие ошибки при поиске VFS
		std::cerr << e.what();
		return nullptr;
	}

	if (VFSPath == TestTask::errorPath) { // не нашли VFS
		VFSPath = VFSInit(filePath);
	}

	VFSInfo info = getVFSInfo(VFSPath);

	if (!info) {
		return nullptr;
	}
	
	File* file = nullptr;

	try {
		int fileCluster = getFileCluster(VFSPath, filePath, TestTask::WriteOnlyMark);

		if (fileCluster == TestTask::didNotFindCluster) {
			fileCluster = addFileToVFS(VFSPath, filePath, TestTask::WriteOnlyMark);
		}
		file = new File(VFSPath, filePath, fileCluster, info.clusterSize, FileStatus::WriteOnly);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		delete file;
		return nullptr;
	}

	return file;
}

size_t TestTask::textFS::Read(File* f, char* buff, size_t len) {

	if (!f) { 
		return 0;
	}

	if (f->status!=FileStatus::ReadOnly) {
		return 0;
	}

	std::ifstream inputStream;
	inputStream.open(f->VFSPath/TestTask::VFSDataFileName, std::ios::binary);
	inputStream.seekg(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);

	size_t maxLength = f->clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	inputStream.read(buff, textLength);
	size_t symbolsRead = textLength;
	f->indicatorPosition += symbolsRead;

	while (symbolsRead < len) {

		try {
			f->currentCluster = findNextCluster(f->VFSPath, f->currentCluster);
			f->indicatorPosition = 0;

			if (f->currentCluster == TestTask::endOfFile) {
				f->status = FileStatus::EndOfFile;
				break;
			}
			else if (f->currentCluster == TestTask::didNotFindCluster ||
					 f->currentCluster == TestTask::faultyCluster) {

				f->status = FileStatus::Bad;
				break;
			}

			inputStream.seekg(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);
			maxLength = f->clusterSize;
			textLength = f->clusterSize >= len - symbolsRead ? len - symbolsRead : f->clusterSize;

			inputStream.read(buff + symbolsRead, textLength);
			symbolsRead += textLength;
			f->indicatorPosition += symbolsRead;
		}
		catch (const std::exception& e) {
			std::cerr << e.what();
			break;
		}
	}
	inputStream.close();
	return symbolsRead;
}

size_t TestTask::textFS::Write(File* f, char* buff, size_t len) {
	if (!f) {
		return 0;
	}

	if (f->status != FileStatus::WriteOnly) {
		return 0;
	}

	std::ofstream outputStream;
	outputStream.open(f->VFSPath / TestTask::VFSDataFileName, std::ios::in | std::ios::out | std::ios::binary);
	outputStream.seekp(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);

	size_t maxLength = f->clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	outputStream.write(buff, textLength);
	size_t symbolsWritten = textLength;
	f->indicatorPosition += symbolsWritten;

	while (symbolsWritten < len) {
		try {
			int nextCluster = findNextCluster(f->VFSPath, f->currentCluster);
			if (nextCluster == TestTask::endOfFile) { // если все кластеры под данный файл закончились, то выделяем новый

				//std::lock_guard(TestTask::VFSCritical);

				VFSInfo info = getVFSInfo(f->VFSPath);

				int currentEmptyCluster = info.FirstEmptyCluster;
				int nextEmptyCluster = findEmptyCluster(f->VFSPath, info.FirstEmptyCluster);
				info.FirstEmptyCluster = nextEmptyCluster;

				refreshVFSHeader(f->VFSPath, info);
				changeClusterAssigment(f->VFSPath, f->currentCluster, currentEmptyCluster);
				changeClusterAssigment(f->VFSPath, currentEmptyCluster, TestTask::endOfFile);
				nextCluster = currentEmptyCluster;
			}
			else if (nextCluster == TestTask::didNotFindCluster) { // не нашли следующий кластер
				break;
			}

			f->currentCluster = nextCluster;
			f->indicatorPosition = 0;

			outputStream.seekp(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);
			maxLength = f->clusterSize;
			textLength = f->clusterSize >= len - symbolsWritten ? len - symbolsWritten : f->clusterSize;

			outputStream.write(buff + symbolsWritten, textLength);
			symbolsWritten += textLength;
			f->indicatorPosition += textLength;
		}
		catch (const std::exception& e) {
			std::cerr << e.what();
			break;
		}
	}
	outputStream.close();
	return symbolsWritten;
}

void TestTask::textFS::Close(File* f) {

	if (!f) {
		return;
	}

	f->currentCluster = f->firstCluster;
	f->status = FileStatus::Closed;
	f->indicatorPosition = 0;

	try {
		deleteModeMark(f->VFSPath, f->filePath);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		return;
	}
};
