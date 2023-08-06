#include "TextFS.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <exception>

TestTask::File::File(std::filesystem::path VFSpath_, std::string filePath_, FileStatus status_)
	: filePath(filePath_), status(status_) {

	VFSHeader.open(VFSpath_ / VFSHeaderFileName, std::ios::in | std::ios::out | std::ios::binary);
	VFSTable.open(VFSpath_ / VFSTableFileName, std::ios::in | std::ios::out | std::ios::binary);
	VFSData.open(VFSpath_ / VFSDataFileName, std::ios::in | std::ios::out | std::ios::binary);

	if (VFSHeader.bad() || VFSTable.bad() || VFSData.bad()) {
		VFSHeader.close();
		VFSTable.close();
		VFSData.close();
		throw std::runtime_error("Could not open VFS\n");
	}
}

TestTask::File::~File() {
	VFSHeader.close();
	VFSTable.close();
	VFSData.close();
}

TestTask::File::operator bool() {
	return bool(clusterSize);
}

/// <summary>
/// Завершение созднания File
/// </summary>
/// <param name="clusterSize_"> - Размер кластера в VFS </param>
/// <param name="firstCluster_"> - Первый кластер файла</param>
void TestTask::File::finInit(int clusterSize_, int firstCluster_) {
	clusterSize = clusterSize_;
	firstCluster = firstCluster_;
	currentCluster = firstCluster_;
}

struct VFSInfo { // структура, в которую будут записываться данные о VFS
	operator bool() { return clusterSize > 0 && FirstEmptyCluster >= 0; }
	int clusterSize = -1;
	int FirstEmptyCluster = -1;
};

struct FileInfo { // структура, в которую будут записываться данные о файле (из VFSHeader)
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

/// <summary>
/// Инициализация VFS
/// </summary>
/// <param name="filePath"> - Путь к файлу</param>
/// <returns>Путь к папке с VFS</returns>
std::filesystem::path VFSInit(const std::string& filePath) { 

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS
	std::lock_guard tableGuard(TestTask::VFSTableAccess);

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

	serviceStream.open(VFSPath / TestTask::VFSTableFileName, std::ios::binary); // в Table записываем данные о первом кластере (первый кластер пустой)
	serviceStream << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
	serviceStream.close();

	serviceStream.open(VFSPath / TestTask::VFSDataFileName, std::ios::binary); // Data файл остается пустым
	serviceStream.close();

	return VFSPath;
}

/// <summary>
/// Поиск пути к файлам VFS
/// </summary>
/// <param name="filePath"> - Путь к файлу</param>
/// <returns>Путь к папке с VFS</returns>
std::filesystem::path findVFSPath(const std::string& filePath) { 

	std::filesystem::path VFSPath(filePath);

	if (VFSPath.empty())
		throw std::runtime_error("Empty path to VFS\n");

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS
	std::lock_guard tableGuard(TestTask::VFSTableAccess);

	while (VFSPath != VFSPath.root_path()) { // в filePath ищем папку, в которой инициализирована VFS
		VFSPath = VFSPath.parent_path();
		if (std::filesystem::exists(VFSPath / TestTask::VFSHeaderFileName) &&
			std::filesystem::exists(VFSPath / TestTask::VFSTableFileName) && 
			std::filesystem::exists(VFSPath / TestTask::VFSDataFileName)) {

			return VFSPath;
		}
	};

	if (VFSPath == VFSPath.root_path()) { // попадем сюда только если не нашли VFS
		VFSPath = TestTask::didNotFindVFS;
	}

	return VFSPath;
}

/// <summary>
/// Получение информации о VFS из VFSHeader
/// </summary>
/// <param name="f"> - File</param>
/// <returns>VFSInfo</returns>
VFSInfo getVFSInfo(TestTask::File* f) {

	if (!f) {
		throw  std::runtime_error("Trying to get info from an empty File\n");
	}

	if (f->VFSHeader.bad()) {
		throw  std::runtime_error("Error while working with VFS header\n");
	}

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess); 

	f->VFSHeader.clear();
	f->VFSHeader.seekg(0, std::ios_base::beg);

	std::string buff;
	VFSInfo info;
	
	std::getline(f->VFSHeader, buff);
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos && !f->VFSHeader.eof()) { // Параметры VFS могут лежать в Header в любом порядке

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
		std::getline(f->VFSHeader, buff);
	}

	return info;
}

/// <summary>
/// Поиск начального кластера файла и отметка об открытии File
/// </summary>
/// <param name="f"> - File</param>
/// <param name="mode"> - режим, в котором будет открыт файл</param>
/// <returns>Номер начального кластера файла</returns>
int openFileThread(TestTask::File* f, const std::string& mode) {
	
	if (f->VFSHeader.bad()) {
		throw  std::runtime_error("Error while working with VFS header\n");
	}

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS

	f->VFSHeader.clear();
	f->VFSHeader.seekg(0, std::ios_base::beg);

	std::string buff;
	int pointerPos = 0;
	FileInfo info(f->getFilePath());

	while (std::getline(f->VFSHeader, buff)) { // ищем нужный файл в header
		if (buff.find(f->getFilePath()) != std::string::npos) { // нашли
			
			f->VFSHeader.seekg(pointerPos, std::ios_base::beg);
			f->VFSHeader >> info;

			if (info.numberOfThreads && buff.find(mode) == std::string::npos) { // если с данным файлом работает хоть один поток в другом режиме
				throw  std::runtime_error("File was already opened in opposing to " + mode + " mode\n");
			}
			else { // либо совпал режим, либо количество рабочих потоков -  ноль
				if (++info.numberOfThreads >TestTask::maxThreadsCount) {
					throw  std::runtime_error("Too many threads for one file\n");
				}
				f->VFSHeader.clear();
				f->VFSHeader.seekp(pointerPos, std::ios_base::beg);
				f->VFSHeader << info; // перед этим увеличилии количество рабочих потоков (см. несколько строк выше)
				f->VFSHeader.flush();
				return info.firstCluster;
			}
		}
		pointerPos = f->VFSHeader.tellp();
	}

	return TestTask::didNotFindCluster; // в том случае, если не нашли файл в VFSHeader
}

/// <summary>
/// Отметка о закрытии File
/// </summary>
/// <param name="f"> - File</param>
void closeFileThread(TestTask::File*f ) { 

	if (!f || !*f) {
		return;
	}

	if (f->VFSHeader.bad()) {
		throw  std::runtime_error("Error while working with VFS header\n");
	}

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS

	f->VFSHeader.clear();
	f->VFSHeader.seekp(0, std::ios_base::beg);
	f->VFSHeader.seekg(0, std::ios_base::beg);

	std::string buff;
	int pointerPos = 0;
	FileInfo info(f->getFilePath());

	while (std::getline(f->VFSHeader, buff)) { // ищем нужный файл в header
		if (buff.find(f->getFilePath()) != std::string::npos) {

			f->VFSHeader.seekg(pointerPos, std::ios_base::beg);
			f->VFSHeader >> info;

			--info.numberOfThreads;

			f->VFSHeader.clear();
			f->VFSHeader.seekp(pointerPos, std::ios_base::beg);
			f->VFSHeader << info;
			f->VFSHeader.flush();
			return;
		}
		pointerPos = f->VFSHeader.tellp();
	}
}

/// <summary>
/// Поиск самого ближнего к from свободного кластера 
/// </summary>
/// <param name="f"> - file</param>
/// <param name="from"> - С какого кластера начинать поиск</param>
/// <returns>Номер свободного кластера</returns>
int findEmptyCluster(TestTask::File* f, int from = 0) { 

	if (!f) {
		throw  std::runtime_error("Trying to get info from an empty File\n");
	}

	if (f->VFSTable.bad()) {
		throw  std::runtime_error("Error while working with VFS table\n");
	}

	std::lock_guard tableGuard(TestTask::VFSTableAccess);// блокируем VFS

	f->VFSTable.clear();
	f->VFSTable.seekg(0, std::ios_base::beg);
	f->VFSTable.seekp(0, std::ios_base::beg);
	
	int clusterStatus = 0;
	int currentLine = 0;
	std::string buff;

	while (!f->VFSTable.eof()) {
		
		f->VFSTable >> clusterStatus;

		if (currentLine > from) {
				if (clusterStatus == TestTask::clusterIsEmpty)
				return currentLine;
		}
		++currentLine;
	}
	
	f->VFSTable.seekp(0, std::ios_base::end);
	f->VFSTable.clear();
	f->VFSTable << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
	f->VFSTable.flush();

	return currentLine;
}

/// <summary>
/// Обновление информации в Header 
/// </summary>
/// <param name="f"> - File</param>
/// <param name="info"> - Информация для записи</param>
void refreshVFSHeader(TestTask::File* f, const VFSInfo& info) {
	// пока что обновляется только позиция первого свободного кластера
	// с расширением возможностей VFS можно обновлять и другие данные

	if (!f) {
		throw  std::runtime_error("Trying to get info from an empty File\n");
	}

	if (f->VFSHeader.bad()) {
		throw  std::runtime_error("Error while working with VFS header\n");
	}

	std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS

	f->VFSHeader.clear();
	f->VFSHeader.seekp(0, std::ios_base::beg);
	f->VFSHeader.seekg(0, std::ios_base::beg);

	std::string buff;
	int pointerPos = 0;
	
	while (buff.find(TestTask::endOfVFSInfo) == std::string::npos && !f->VFSHeader.eof() && std::getline(f->VFSHeader, buff)) {

		if (buff.find(TestTask::firstEmptyClusterMark) != std::string::npos) { // работает - не чини
			f->VFSHeader.seekp(pointerPos, std::ios::beg);
			f->VFSHeader << TestTask::firstEmptyClusterMark + std::string(TestTask::maxSettingLength - TestTask::firstEmptyClusterMark.length(), ' ');
			f->VFSHeader << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << info.FirstEmptyCluster;
			f->VFSHeader << '\n';
			f->VFSHeader.flush();
			break;
		}
		pointerPos = f->VFSHeader.tellp();
 	}
}

/// <summary>
/// Переназначение ссылки на следующий кластер
/// </summary>
/// <param name="f"> - File</param>
/// <param name="clusterNumber"> - Откуда ссылаемся</param>
/// <param name="changeTo"> - Куда ссылаемся</param>
void changeClusterAssigment(TestTask::File* f, int clusterNumber, int changeTo) {

	if (clusterNumber < 0) {
		throw  std::runtime_error("Invalid cluster number\n");
	}

	if (!f) {
		throw  std::runtime_error("Trying to get info from an empty File\n");
	}

	if (f->VFSTable.bad()) {
		throw  std::runtime_error("Error while working with VFS table\n");
	}

	std::lock_guard tableGuard(TestTask::VFSTableAccess); // блокируем VFS

	f->VFSTable.clear();
	f->VFSTable.seekp(0, std::ios_base::beg);
	f->VFSTable.seekg(0, std::ios_base::beg);

	int currentCluster = 0;
	int pointerPos = 0;
	std::string buff;

	while (std::getline(f->VFSTable,buff)) {
		
		int currentAssigment = std::stoi(buff);

		if (currentCluster == clusterNumber) {
			f->VFSTable.seekp(pointerPos, std::ios::beg);
			f->VFSTable.clear();

			if (changeTo >= 0) {
				f->VFSTable << std::setw(TestTask::maxClusterDigits) << std::setfill('0') << changeTo << '\n';
			}
			else {
				f->VFSTable << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(changeTo) << '\n';
			}
			f->VFSTable.flush();
			break;
		}
		++currentCluster;
		pointerPos = f->VFSTable.tellp();
	}

	if (currentCluster == clusterNumber && f->VFSTable.eof() && changeTo == TestTask::clusterIsEmpty) { // если нужно проставить метку пустого кластера в конец файла 
		f->VFSTable.clear();
		f->VFSTable.seekp(0, std::ios::end);
		f->VFSTable << "-" << std::setw(TestTask::maxClusterDigits - 1) << std::setfill('0') << std::abs(TestTask::clusterIsEmpty) << '\n';
		f->VFSTable.flush();
	}

	if (currentCluster < clusterNumber) {
		throw  std::runtime_error("Error while changing cluster assigment\n");
	}
}

/// <summary>
/// Поиск следующего кластера
/// </summary>
/// <param name="f"> - File</param>
/// <returns>Номер следующего кластера</returns>
int findNextCluster(TestTask::File* f) {

	if (!f) {
		throw  std::runtime_error("Trying to get info from an empty File\n");
	}

	if (f->VFSTable.bad()) {
		throw  std::runtime_error("Error while working with VFS table\n");
	}

	std::lock_guard tableGuard(TestTask::VFSTableAccess);

	f->VFSTable.clear();
	f->VFSTable.seekp(0, std::ios_base::beg);
	f->VFSTable.seekg(0, std::ios_base::beg);

	int clusterAssigment = 0;
	int currentLine = 0;

	while (!f->VFSTable.eof()) {

		f->VFSTable >> clusterAssigment;

		if (currentLine == f->currentCluster) {
				return clusterAssigment;
		}
		++currentLine;
	}

	return TestTask::didNotFindCluster;
}

/// <summary>
/// Добавление файла в VFS
/// </summary>
/// <param name="f"> - File</param>
/// <param name="mode"> - Режим, в котором будет открыт файл</param>
/// <returns>Номер первого кластера файла</returns>
int addFileToVFS(TestTask::File* f, const std::string& mode) { // добавляем файл в VFS

	try {
		
		VFSInfo info = getVFSInfo(f);

		int currentEmptyCluster = info.FirstEmptyCluster;
		int nextEmptyCluster = findEmptyCluster(f, info.FirstEmptyCluster);
		info.FirstEmptyCluster = nextEmptyCluster;

		refreshVFSHeader(f, info);
		changeClusterAssigment(f, currentEmptyCluster, TestTask::endOfFile);
		changeClusterAssigment(f, nextEmptyCluster, TestTask::clusterIsEmpty);

		if (f->VFSHeader.bad()) {
			throw  std::runtime_error("Error while working with VFS header\n");
		}

		std::lock_guard headerGuard(TestTask::VFSHeaderAccess);// блокикуем VFS (важно делать после refreshVFSHeader)

		f->VFSHeader.clear();
		f->VFSHeader.seekp(0, std::ios_base::end);
		FileInfo fileInfo(f->getFilePath(), currentEmptyCluster, mode, 1);
		
		f->VFSHeader << fileInfo;
		f->VFSHeader.flush();

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

	if (VFSPath == TestTask::didNotFindVFS) {
		return nullptr;
	}

	File* file = new File(VFSPath,filePath,FileStatus::ReadOnly);

	VFSInfo info = getVFSInfo(file);

	if (!info) {
		return nullptr;
	}

	try {
		int fileCluster = openFileThread(file, ReadOnlyMark);
		if (fileCluster == TestTask::didNotFindCluster ) {
			delete file;
			return nullptr;
		}
		file->finInit(info.clusterSize, fileCluster);
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
	catch (const std::exception& e) {
		std::cerr << e.what();
		return nullptr;
	}

	if (VFSPath == TestTask::didNotFindVFS) {
		VFSPath = VFSInit(filePath);
	}

	File* file = new File(VFSPath, filePath, FileStatus::WriteOnly);

	VFSInfo info = getVFSInfo(file);

	if (!info) {
		return nullptr;
	}

	try {
		int fileCluster = openFileThread(file, TestTask::WriteOnlyMark);

		if (fileCluster == TestTask::didNotFindCluster) {
			fileCluster = addFileToVFS(file, TestTask::WriteOnlyMark);
		}
		file->finInit(info.clusterSize, fileCluster);
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		delete file;
		return nullptr;
	}

	return file;
}

size_t TestTask::textFS::Read(File* f, char* buff, size_t len) {

	if (!f || f->VFSData.bad()) {
		return 0;
	}

	if (f->getStatus()!=FileStatus::ReadOnly) {
		return 0;
	}

	size_t clusterSize = f->getClusterSize();
	size_t maxLength = clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	f->VFSData.seekg(f->currentCluster * clusterSize + f->indicatorPosition, std::ios::beg);
	f->VFSData.read(buff, textLength);
	size_t symbolsRead = textLength;
	f->indicatorPosition += symbolsRead;

	while (symbolsRead < len) {

		try {
			f->currentCluster = findNextCluster(f);
			f->indicatorPosition = 0;

			if (f->currentCluster == TestTask::endOfFile) {
				f->setEOFStatus();
			}
			else
			if (f->currentCluster == TestTask::didNotFindCluster ||
				f->currentCluster == TestTask::faultyCluster) {

				f->setBadStatus();
				break;
			}

			f->VFSData.seekg(f->currentCluster * clusterSize + f->indicatorPosition, std::ios::beg);
			maxLength = clusterSize;
			textLength = clusterSize >= len - symbolsRead ? len - symbolsRead : clusterSize;

			f->VFSData.read(buff + symbolsRead, textLength);
			symbolsRead += textLength;
			f->indicatorPosition += symbolsRead;
		}
		catch (const std::exception& e) {
			std::cerr << e.what();
			break;
		}
	}
	return symbolsRead;
}

size_t TestTask::textFS::Write(File* f, char* buff, size_t len) {
	if (!f) {
		return 0;
	}

	if (f->getStatus() != FileStatus::WriteOnly) {
		return 0;
	}

	size_t clusterSize = f->getClusterSize();
	f->VFSData.seekp(f->currentCluster * clusterSize + f->indicatorPosition, std::ios::beg);

	size_t maxLength = clusterSize - f->indicatorPosition; // максимальное количество символов, которое может поместиться в текущий кластер
	size_t textLength = maxLength >= len ? len : maxLength;

	f->VFSData.write(buff, textLength);
	size_t symbolsWritten = textLength;
	f->indicatorPosition += symbolsWritten;

	while (symbolsWritten < len) {
		try {
			int nextCluster = findNextCluster(f);
			if (nextCluster == TestTask::endOfFile) { // если все кластеры под данный файл закончились, то выделяем новый

				VFSInfo info = getVFSInfo(f);

				int currentEmptyCluster = info.FirstEmptyCluster;
				int nextEmptyCluster = findEmptyCluster(f, info.FirstEmptyCluster);
				info.FirstEmptyCluster = nextEmptyCluster;

				refreshVFSHeader(f, info);
				changeClusterAssigment(f, f->currentCluster, currentEmptyCluster); // с текущего ссылаемся на только что выделенный
				changeClusterAssigment(f, currentEmptyCluster, TestTask::endOfFile); // только что выделенный помечаем как конец файла
				changeClusterAssigment(f, nextEmptyCluster, TestTask::clusterIsEmpty); // следующий пустой кластер помечаем как пустой
				nextCluster = currentEmptyCluster;
			}
			else if (nextCluster == TestTask::didNotFindCluster) { // не нашли следующий кластер
				break;
			}

			f->currentCluster = nextCluster;
			f->indicatorPosition = 0;

			f->VFSData.seekp(f->currentCluster * clusterSize + f->indicatorPosition, std::ios::beg);
			maxLength = clusterSize;
			textLength = clusterSize >= len - symbolsWritten ? len - symbolsWritten : clusterSize;

			f->VFSData.write(buff + symbolsWritten, textLength);
			symbolsWritten += textLength;
			f->indicatorPosition += textLength;
		}
		catch (const std::exception& e) {
			std::cerr << e.what();
			break;
		}
	}
	f->VFSData.flush();
	return symbolsWritten;
}

void TestTask::textFS::Close(File* f) {

	if (!f) {
		return;
	}

	f->currentCluster = f->getFirstCluster();
	f->setClosedStatus();
	f->indicatorPosition = 0;

	try {
		closeFileThread(f);
		delete f;
	}
	catch (const std::exception& e) {
		std::cerr << e.what();
		return;
	}
};
