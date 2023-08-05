#pragma once
#include <string>
#include <filesystem>
#include <mutex>
#include <fstream>

namespace TestTask {

	inline const std::string VFSFileFormat(".dat");
	inline const std::string VFSHeaderFileName("VFSHeader" + VFSFileFormat);
	inline const std::string VFSTableFileName("VFSTable" + VFSFileFormat);
	inline const std::string VFSDataFileName("VFSData" + VFSFileFormat);

	inline const std::string clusterSizeMark("ClusterSize =");
	inline const std::string firstEmptyClusterMark("FirstEmptyCluster =");
	inline const std::string endOfVFSInfo("-----");
	inline const std::string WriteOnlyMark("WO");
	inline const std::string ReadOnlyMark("RO");

	inline const std::filesystem::path didNotFindVFS("didNotFindVFS");

	inline const int maxClusterDigits = 16; //максимальная длина номера кластера 
	inline const int maxSettingLength = 50; // максимальная длина названия свойсва VFS
	inline const int maxModeMarkLength = 4; // максимальная длина кода режима работы файла
	inline const int maxThreadsCount = 20; // максимальное количество потоков на один файл
	inline const int maxThreadsCounterLength = 2; // количество цифр в maxThreadsCount
	inline const int defaultClusterSize = 10; // количество символов на один кластер

	// метки для VFSTable
	inline const int clusterIsEmpty = -1; // метка пустого кластера
	inline const int endOfFile = -2; // метка последнего для данного файла кластера
	inline const int faultyCluster = -3; // метка кластера с ошибкой 
	inline const int didNotFindCluster = -4; // ошибка при поиске кластера

	static std::mutex VFSTableAccess;
	static std::mutex VFSHeaderAccess;

	enum class FileStatus : char {
		ReadOnly,WriteOnly,Closed,EndOfFile,Bad
	};

	class File {
	public:
		std::fstream VFSHeader;

		std::fstream VFSTable;

		std::fstream VFSData;

		size_t indicatorPosition = 0; // позиция курсора в текущем кластере

		size_t currentCluster = 0; // номер текущего кластера

		File(std::filesystem::path VFSpath_, std::string filePath_, FileStatus status_);

		~File();

		operator bool();

		std::string getFilePath() { return filePath; };

		size_t getClusterSize() { return clusterSize; }

		void finInit(int clusterSize_, int firstCluster_);

		FileStatus getStatus() { return status; }

		void setClosedStatus() { status = FileStatus::Closed; }

		void setBadStatus() { status = FileStatus::Bad; }

		void setEOFStatus() { status = FileStatus::EndOfFile; }

		size_t getFirstCluster() { return firstCluster; }

	private:
		FileStatus status = FileStatus::Closed;

		//std::filesystem::path VFSPath; // путь к директории с VFS
		std::string filePath; // "фиктивный" путь к файлу

		size_t firstCluster = 0; // номер первого кластера данного файла

		size_t clusterSize = 0;
	};

	struct IVFS {
		virtual File* Open(const char* name) = 0; // Открыть файл в readonly режиме. Если нет такого файла или же он открыт во writeonly режиме - вернуть nullptr
		virtual File* Create(const char* name) = 0; // Открыть или создать файл в writeonly режиме. Если нужно, то создать все нужные поддиректории, упомянутые в пути. Вернуть nullptr, если этот файл уже открыт в readonly режиме.
		virtual size_t Read(File* f, char* buff, size_t len) = 0; // Прочитать данные из файла. Возвращаемое значение - сколько реально байт удалось прочитать
		virtual size_t Write(File* f, char* buff, size_t len) = 0; // Записать данные в файл. Возвращаемое значение - сколько реально байт удалось записать
		virtual void Close(File* f) = 0; // Закрыть файл	
	};
}
