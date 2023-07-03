// -*- coding: utf-8 -*-
#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <memory>

namespace TestTask {

	inline const std::string packFileFormat(".dat");
	inline const std::string headerFileName("header" + packFileFormat);
	inline const int defaultFilesPerPack = 2; // количество файлов в одном .dat файле (в одной пачке)
	inline const int defaultFileOffset = 10; // количество символов на один файл

	// дефолтные значения поставил такими маленькими чтобы было видно:
	// создание нескольких пачек 
	// попытку записи за границы файла
	// 

	// int в unordered map - количество потоков, работающих с данным файлом ( количество объектов File открытых на этот файл)
	static std::unordered_map<std::string, int> writeOnlyFiles;
	static std::unordered_map<std::string, int> readOnlyFiles;
	static std::unordered_map<std::string, std::unique_ptr<std::mutex>> fileMutexes;

	static std::mutex mapAccess;

	struct File {
		std::filesystem::path packFileName; // путь к .dat файлу (к пачке)
		std::string filePath; // "фиктивный" путь к файлу
		size_t fileOffset; // количество символов на один файл внутри конкретного .dat файла (внутри той пачки, в которой лежит файл)
		size_t filePosition; // позиция файла внутри конкретного .dat файла (внутри той пачки, в которой лежит файл)
		size_t indicatorPosition; // позиция, с которой будем читать/записывать
	};

	struct IVFS {
		virtual File* Open(const char* name) = 0; // Открыть файл в readonly режиме. Если нет такого файла или же он открыт во writeonly режиме - вернуть nullptr
		virtual File* Create(const char* name) = 0; // Открыть или создать файл в writeonly режиме. Если нужно, то создать все нужные поддиректории, упомянутые в пути. Вернуть nullptr, если этот файл уже открыт в readonly режиме.
		virtual size_t Read(File* f, char* buff, size_t len) = 0; // Прочитать данные из файла. Возвращаемое значение - сколько реально байт удалось прочитать
		virtual size_t Write(File* f, char* buff, size_t len) = 0; // Записать данные в файл. Возвращаемое значение - сколько реально байт удалось записать
		virtual void Close(File* f) = 0; // Закрыть файл	
	};
}
