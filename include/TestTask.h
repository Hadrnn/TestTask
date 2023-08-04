#pragma once
#include <string>
#include <filesystem>
#include <mutex>

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

	static std::mutex VFSCritical;

	enum class FileStatus : char {
		ReadOnly,WriteOnly,Closed,EndOfFile,Bad
	};

	struct File {
		FileStatus status = FileStatus::Closed;
		std::filesystem::path VFSPath; // путь к директории с VFS
		std::string filePath; // "фиктивный" путь к файлу
		int firstCluster = 0; // номер первого кластера данного файла
		int currentCluster = 0; // номер текущего кластера
		size_t clusterSize = 0;
		size_t indicatorPosition = 0; // позиция курсора в текущем кластере

		File(std::filesystem::path VFSpath_, std::string filePath_, int Cluster_, int clusterSize_, FileStatus status_)
			: VFSPath(VFSpath_), filePath(filePath_), firstCluster(Cluster_),currentCluster(Cluster_), clusterSize(clusterSize_), status(status_) {}
	};

	struct IVFS {
		virtual File* Open(const char* name) = 0; // Открыть файл в readonly режиме. Если нет такого файла или же он открыт во writeonly режиме - вернуть nullptr
		virtual File* Create(const char* name) = 0; // Открыть или создать файл в writeonly режиме. Если нужно, то создать все нужные поддиректории, упомянутые в пути. Вернуть nullptr, если этот файл уже открыт в readonly режиме.
		virtual size_t Read(File* f, char* buff, size_t len) = 0; // Прочитать данные из файла. Возвращаемое значение - сколько реально байт удалось прочитать
		virtual size_t Write(File* f, char* buff, size_t len) = 0; // Записать данные в файл. Возвращаемое значение - сколько реально байт удалось записать
		virtual void Close(File* f) = 0; // Закрыть файл	
	};
}
