#include "TextFS.h"
#include <iostream>
#include <fstream>
#include <string>
#include <exception>

const std::string spacer("                  \n");

struct VFSInfo { // структурка, в которую будем записывать данные о VFS
	operator bool() { return clusterSize > 0 && FirstEmptyCluster >= 0; }
	static const int VFSArgsAmount = 2;
	int clusterSize = -1;
	int FirstEmptyCluster = -1;
};

std::filesystem::path VFSInit(std::string filePath) { // инициализыция

	//std::lock_guard(TestTask::VFSCritical);// блокикуем VFS

	std::filesystem::path VFSPath(filePath);

	while (!std::filesystem::exists(VFSPath) && VFSPath != VFSPath.root_path()) { // ищем существющую папку из filePath (первой найдется та, что "глубже" лежит)
		VFSPath = VFSPath.parent_path();
	}
	
	std::ofstream serviceStream;     // создаем три файла, которые необходимы для работы VFS
	serviceStream.open(VFSPath / TestTask::VFSHeaderFileName);  // в Header записываем данные о VFS
	serviceStream << TestTask::clusterSizeMark << TestTask::defaultClusterSize << spacer;
	serviceStream << TestTask::firstEmptyClusterMark << 0 << spacer;
	serviceStream << TestTask::endOfVFSInfo << spacer;
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSTableFileName); // в Table записываем данные о первом кластере (он пустой)
	serviceStream << TestTask::clusterIsEmpty << spacer;
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSDataFileName); // Data файл остается пустым
	serviceStream.close();

	return VFSPath;
}

std::filesystem::path findVFSPath(std::string filePath) {

	std::filesystem::path VFSPath(filePath);

	if (VFSPath.empty())
		throw std::runtime_error("Empty path to VFS");


	while (VFSPath != VFSPath.root_path()) { // в filePath ищем папку, в которой инициализирована VFS
		if (std::filesystem::exists(VFSPath / TestTask::VFSHeaderFileName) &&
			std::filesystem::exists(VFSPath / TestTask::VFSTableFileName) && 
			std::filesystem::exists(VFSPath / TestTask::VFSDataFileName)) {

			std::ifstream serviceStream;  // проверяем целостность файлов
			
			serviceStream.open(VFSPath / TestTask::VFSHeaderFileName);
			if(!serviceStream.good())
				throw std::runtime_error("Could not open VFS Header");
			serviceStream.close();

			serviceStream.open(VFSPath / TestTask::VFSTableFileName);
			if (!serviceStream.good())
				throw std::runtime_error("Could not open VFS Table");
			serviceStream.close();

			serviceStream.open(VFSPath / TestTask::VFSDataFileName);
			if (!serviceStream.good())
				throw std::runtime_error("Could not open VFS Data file");
			serviceStream.close();

			break;
		}
		VFSPath = VFSPath.parent_path();
	}

	if (VFSPath == VFSPath.root_path())
		throw std::logic_error("Did not find VFS");

	return VFSPath;
}

VFSInfo getVFSInfo(std::filesystem::path VFSPath) { // получаем информацию о VFS из Header

	std::ifstream header;
	std::string buff;

	VFSInfo info;

	header.open(VFSPath / TestTask::VFSHeaderFileName);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header");
	}
	
	
	std::getline(header, buff);
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos && !header.eof()) { // Параметры VFS могут лежать в Header в любом порядке

		if (buff.find(TestTask::clusterSizeMark) != std::string::npos) {
			try {
				info.clusterSize = std::stoi(buff.substr(TestTask::clusterSizeMark.length(), buff.length()));
			}
			catch (const std::exception&) {
				std::cerr << "Error while working with VFS Header\n";
			}
		}
		else if (buff.find(TestTask::firstEmptyClusterMark) != std::string::npos) {
			try {
				info.FirstEmptyCluster = std::stoi(buff.substr(TestTask::firstEmptyClusterMark.length(),buff.length()));
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

bool hasModeMark(const std::string& str) { // для понимания, есть ли в Header информация о режиме, в котором открыт файл
	for (char c : str) {
		if (c != ' ' && c != '\n') {
			return true;
		}
	}
	return false;
}

int getFileCluster(std::filesystem::path VFSPath, std::string fileName,std::string mode) { // ищем изначальный кластер файла
	
	std::string buff;
	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header");
	}

	int fileCluster = -1;
	size_t indicatorPosition = 0;

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) {
			std::string buff2 = buff;
			buff = buff.substr(fileName.length() + 1, buff.length());

			fileCluster = std::stoi(buff); // получили номер кластера
			buff = buff.substr(std::to_string(fileCluster).length(), buff.length());

			if (hasModeMark(buff)) {
				if (buff.find(mode) == std::string::npos) {
					throw  std::runtime_error("File was already opened in opposing to " + mode + " mode"); // проверка на то, что файл открыт в необходиимом режиме
				}
			}// реализовано не до конца. Для работы этой особенности VFS необходимо ...
			else {
				header.seekp(indicatorPosition + buff2.find_first_of(std::to_string(fileCluster)) + std::to_string(fileCluster).length() + 2, std::ios_base::beg);
				header << mode;
			}

			break;
		}
		indicatorPosition += buff.length() + 1;
	}

	if (fileCluster < 0)
		throw  std::logic_error("Did not find a file in VFS");

	header.close();
	return fileCluster;
}

void deleteModeMark(std::filesystem::path VFSPath, std::string fileName) {

	std::string buff;
	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header");
	}

	int fileCluster = -1;
	size_t indicatorPosition = 0;

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) {
			std::string buff2 = buff;
			buff = buff.substr(fileName.length() + 1, buff.length());

			fileCluster = std::stoi(buff);
			buff = buff.substr(std::to_string(fileCluster).length(), buff.length());

			header.seekp(indicatorPosition + buff2.find_first_of(std::to_string(fileCluster)) + std::to_string(fileCluster).length() + 2, std::ios_base::beg);
			header << spacer;

			return;
		}
		indicatorPosition += buff.length() + 1;
	}
}


int findEmptyCluster(std::filesystem::path VFSPath, int from = 0) { // поиск самого ближнего к началу Data фала свободного кластера 
	std::fstream serviceStream;


	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out);
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table");
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
	serviceStream << TestTask::clusterIsEmpty << spacer;
	return currentLine;
}

void refreshVFSHeader(std::filesystem::path VFSPath, VFSInfo info) {
	// обновляем информацию в Header (пока что обновляется только позиция первого свободного кластера)
	// с расширением возможностей VFS можно обновлять и другие данные

	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out );
	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header");
	}
	
	std::string buff;
	size_t indicatorPosition = 0;
	std::getline(header, buff);
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos) {


		 if (buff.find(TestTask::firstEmptyClusterMark) != std::string::npos) { // работает - не чини
			header.seekp(indicatorPosition + TestTask::firstEmptyClusterMark.length(), std::ios::beg);
			header.clear();
			header << info.FirstEmptyCluster;
			header.seekp(indicatorPosition, std::ios::beg);
			std::getline(header, buff);
		}
		indicatorPosition += buff.length() + 1;
		std::getline(header, buff);
 	}
}

void changeClusterAssigment(std::filesystem::path VFSPath, int clusterNumber, int changeTo) { // переназначаем ссылку на следующий кластер
	std::fstream serviceStream;
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out);
	
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table");
	}

	int currentCluster = 0;
	
	while (!serviceStream.eof()) {
		
		int currentAssigment = 0;
		serviceStream >> currentAssigment;

		if (currentCluster == clusterNumber) {
			serviceStream.seekp(-int(std::to_string(currentAssigment).length()), std::ios::cur);
			serviceStream << changeTo << spacer;
			break;
		}
		++currentCluster;
	}
	serviceStream.close();

	if (currentCluster < clusterNumber) {
		throw  std::runtime_error("Error while changing cluster assigment");
	}
}

int findNextCluster(std::filesystem::path VFSPath, int cluster) { // переходим по ссылку на следующий кластер 
	std::fstream serviceStream;
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out );
	if (!serviceStream.good()) {
		serviceStream.close();
		throw  std::runtime_error("Couldnt open VFS Table");
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

	throw  std::logic_error("Didnt find a cluster in VFS Table");
	return 0;
}

int addFileToVFS(std::filesystem::path VFSPath, std::string fileName) { // добавляем файл в VFS

	try {
		//std::lock_guard(TestTask::VFSCritical);

		VFSInfo info = getVFSInfo(VFSPath);

		int currentEmptyCluster = info.FirstEmptyCluster;
		int nextEmptyCluster = findEmptyCluster(VFSPath, info.FirstEmptyCluster);
		info.FirstEmptyCluster = nextEmptyCluster;

		refreshVFSHeader(VFSPath, info);
		changeClusterAssigment(VFSPath, currentEmptyCluster, TestTask::endOfFile);

		std::fstream header;
		header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::ate);
		if (!header.good()) {
			header.close();
			throw  std::runtime_error("Couldnt open VFS Header");
		}
		header.seekp(0, std::ios_base::end);
		header << fileName << " " << currentEmptyCluster << spacer;
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

	VFSInfo info = getVFSInfo(VFSPath);

	if (!info) {
		return nullptr;
	}

	File* file = nullptr;

	try {
		file = new File(VFSPath, filePath, getFileCluster(VFSPath, filePath, ReadOnlyMark), info.clusterSize, FileStatus::ReadOnly);
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
	catch (const std::logic_error&) { // не нашли VFS
		VFSPath = VFSInit(filePath);
	}
	catch (const std::exception& e) { // другие ошибки при поиске VFS
		std::cerr << e.what();
		return nullptr;
	}

	VFSInfo info = getVFSInfo(VFSPath);

	if (!info) {
		return nullptr;
	}
	
	File* file = nullptr;

	try {
		file = new File(VFSPath, filePath, getFileCluster(VFSPath, filePath, WriteOnlyMark), info.clusterSize, FileStatus::WriteOnly);
	}
	catch (const std::logic_error&) { // файла не было в VFS
		try {
			file = new File(VFSPath, filePath, addFileToVFS(VFSPath, filePath), info.clusterSize, FileStatus::WriteOnly);
		}
		catch (const std::exception& e) { // другие ошибки 
			std::cerr << e.what();
			delete file;
			return nullptr;
		}
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
	inputStream.open(f->VFSPath/TestTask::VFSDataFileName);
	inputStream.seekg(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);

	size_t maxLength = f->clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	inputStream.read(buff, textLength);
	size_t symbolsRead = textLength;
	f->indicatorPosition += symbolsRead;

	while (symbolsRead < len) {

		f->currentCluster = findNextCluster(f->VFSPath, f->currentCluster);
		f->indicatorPosition = 0;

		if (f->currentCluster == TestTask::endOfFile) {
			f->status = FileStatus::EndOfFile;
			break;
		}
		
		inputStream.seekg(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);
		maxLength = f->clusterSize;
		textLength = f->clusterSize >= len - symbolsRead ? len - symbolsRead : f->clusterSize;

		inputStream.read(buff+symbolsRead, textLength);
		symbolsRead += textLength;
		f->indicatorPosition += symbolsRead;
	}
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
	outputStream.open(f->VFSPath / TestTask::VFSDataFileName, std::ios::in | std::ios::out );
	outputStream.seekp(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);

	size_t maxLength = f->clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	outputStream.write(buff, textLength);
	size_t symbolsWritten = textLength;
	f->indicatorPosition += symbolsWritten;

	while (symbolsWritten < len) {
		
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

		f->currentCluster = nextCluster;
		f->indicatorPosition = 0;

		outputStream.seekp(f->currentCluster * f->clusterSize + f->indicatorPosition, std::ios::beg);
		maxLength = f->clusterSize;
		textLength = f->clusterSize >= len - symbolsWritten ? len - symbolsWritten : f->clusterSize;

		outputStream.write(buff + symbolsWritten, textLength);
		symbolsWritten += textLength;
		f->indicatorPosition += textLength;
	}
	return symbolsWritten;
}

void TestTask::textFS::Close(File* f) {

	if (!f) { 
		return;
	}

	f->currentCluster = f->firstCluster;
	f->status = FileStatus::Closed;
	f->indicatorPosition = 0;

	deleteModeMark(f->VFSPath, f->filePath);
};
