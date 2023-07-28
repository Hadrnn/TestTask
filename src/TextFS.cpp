#include "TextFS.h"
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

std::filesystem::path VFSInit(std::string filePath) { // инициализыция

	//std::lock_guard(TestTask::VFSCritical);// блокикуем VFS

	std::filesystem::path VFSPath(filePath);

	while (!std::filesystem::exists(VFSPath) && VFSPath != VFSPath.root_path()) { // ищем существющую папку из filePath (первой найдется та, что "глубже" лежит)
		VFSPath = VFSPath.parent_path();
	}
	
	std::ofstream serviceStream;     // создаем три файла, которые необходимы для работы VFS
	serviceStream.open(VFSPath / TestTask::VFSHeaderFileName);  // в Header записываем данные о VFS
	serviceStream << TestTask::clusterSizeMark + std::string(TestTask::maxSettingLength - TestTask::clusterSizeMark.length(), ' ');
	serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << TestTask::defaultClusterSize << '\n';
	serviceStream << TestTask::firstEmptyClusterMark + std::string(TestTask::maxSettingLength - TestTask::firstEmptyClusterMark.length(), ' ');
	serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << 0 << '\n';
	serviceStream << TestTask::endOfVFSInfo << '\n';
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSTableFileName); // в Table записываем данные о первом кластере (он пустой)
	if (TestTask::clusterIsEmpty >= 0) { // if else на случай, если переделать clusterIsEmpty на некое положительное число
		serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << TestTask::clusterIsEmpty << '\n';
	}
	else {
		serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
	}
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSDataFileName); // Data файл остается пустым
	serviceStream.close();

	return VFSPath;
}

std::filesystem::path findVFSPath(std::string filePath) {

	std::filesystem::path VFSPath(filePath);

	if (VFSPath.empty())
		throw std::runtime_error("Empty path to VFS\n");


	while (VFSPath != VFSPath.root_path()) { // в filePath ищем папку, в которой инициализирована VFS
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

			break;
		}
		VFSPath = VFSPath.parent_path();
	}

	if (VFSPath == VFSPath.root_path())
		throw std::logic_error("Did not find VFS\n");

	return VFSPath;
}

VFSInfo getVFSInfo(std::filesystem::path VFSPath) { // получаем информацию о VFS из Header

	std::ifstream header;
	std::string buff;

	VFSInfo info;

	header.open(VFSPath / TestTask::VFSHeaderFileName);

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
		throw  std::runtime_error("Couldnt open VFS header\n");
	}

	int fileCluster = -1;
	int pointerPos = 0;


	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) {
			int lineLength = buff.length();
			buff = buff.substr(fileName.length() + 1, buff.length());

			fileCluster = std::stoi(buff); // получили номер кластера
			buff = buff.substr(TestTask::maxClusterDigits + 1, buff.length());

			if (hasModeMark(buff)) {
				if (buff.find(mode) == std::string::npos) {
					throw  std::runtime_error("File was already opened in opposing to " + mode + " mode\n"); // проверка на то, что файл открыт в необходиимом режиме
				}
			}
			else {
				header.clear();
				header.seekp(pointerPos + fileName.length() + 1 + TestTask::maxClusterDigits + 1, std::ios_base::beg);
				header << mode + std::string(TestTask::maxModeMarkLength - mode.length(), ' ');
			}

			break;
		}
		pointerPos = header.tellp();
	}

	if (fileCluster < 0)
		throw  std::logic_error("Did not find a file in VFS\n");

	header.close();
	return fileCluster;
}

void deleteModeMark(std::filesystem::path VFSPath, std::string fileName) {

	std::string buff;
	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out);

	if (!header.good()) {
		header.close();
		throw  std::runtime_error("Couldnt open VFS header\n");
	}

	int fileCluster = -1;

	int pointerPos = 0;

	while (std::getline(header, buff)) { // ищем нужный файл в header
		if (buff.find(fileName) != std::string::npos) {
			
			int lineLength = buff.length();

			header.clear();
			header.seekp(pointerPos + fileName.length() + 1 + TestTask::maxClusterDigits + 1, std::ios_base::beg);
			header << std::string(TestTask::maxModeMarkLength, ' ');
			return;
		}
		pointerPos = header.tellp();
	}
	header.close();
}

int findEmptyCluster(std::filesystem::path VFSPath, int from = 0) { // поиск самого ближнего к началу Data фала свободного кластера 
	std::fstream serviceStream;


	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out);
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
	if (TestTask::clusterIsEmpty >= 0) {
		serviceStream << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << TestTask::clusterIsEmpty << '\n';
	}
	else {
		serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
	}

	return currentLine;
}

void refreshVFSHeader(std::filesystem::path VFSPath, VFSInfo info) {
	// обновляем информацию в Header (пока что обновляется только позиция первого свободного кластера)
	// с расширением возможностей VFS можно обновлять и другие данные

	std::fstream header;
	header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out );
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
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out);
	
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
	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::in | std::ios::out );
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

	throw  std::logic_error("Didnt find a cluster in VFS Table\n");
	return 0;
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
		header.open(VFSPath / TestTask::VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::ate);
		if (!header.good()) {
			header.close();
			throw  std::runtime_error("Couldnt open VFS Header\n");
		}

		header.seekp(0, std::ios_base::end);
		header << fileName << " ";
		header << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << currentEmptyCluster;
		header << ' ' << mode + std::string(TestTask::maxModeMarkLength - mode.length(), ' ') << '\n';
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
			file = new File(VFSPath, filePath, addFileToVFS(VFSPath, filePath,TestTask::WriteOnlyMark), info.clusterSize, FileStatus::WriteOnly);
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
