// -*- coding: utf-8 -*-
#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <memory>

namespace TestTask {

	inline const std::string packFileFormat(".dat");
	inline const std::string headerFileName("header" + packFileFormat);
	inline const int defaultFilesPerPack = 2; // ���������� ������ � ����� .dat ����� (� ����� �����)
	inline const int defaultFileOffset = 10; // ���������� �������� �� ���� ����

	// ��������� �������� �������� ������ ���������� ����� ���� �����:
	// �������� ���������� ����� 
	// ������� ������ �� ������� �����
	// 

	// int � unordered map - ���������� �������, ���������� � ������ ������ ( ���������� �������� File �������� �� ���� ����)
	static std::unordered_map<std::string, int> writeOnlyFiles;
	static std::unordered_map<std::string, int> readOnlyFiles;
	static std::unordered_map<std::string, std::unique_ptr<std::mutex>> fileMutexes;

	static std::mutex mapAccess;

	struct File {
		std::filesystem::path packFileName; // ���� � .dat ����� (� �����)
		std::string filePath; // "���������" ���� � �����
		size_t fileOffset; // ���������� �������� �� ���� ���� ������ ����������� .dat ����� (������ ��� �����, � ������� ����� ����)
		size_t filePosition; // ������� ����� ������ ����������� .dat ����� (������ ��� �����, � ������� ����� ����)
		size_t indicatorPosition; // �������, � ������� ����� ������/����������
	};

	struct IVFS {
		virtual File* Open(const char* name) = 0; // ������� ���� � readonly ������. ���� ��� ������ ����� ��� �� �� ������ �� writeonly ������ - ������� nullptr
		virtual File* Create(const char* name) = 0; // ������� ��� ������� ���� � writeonly ������. ���� �����, �� ������� ��� ������ �������������, ���������� � ����. ������� nullptr, ���� ���� ���� ��� ������ � readonly ������.
		virtual size_t Read(File* f, char* buff, size_t len) = 0; // ��������� ������ �� �����. ������������ �������� - ������� ������� ���� ������� ���������
		virtual size_t Write(File* f, char* buff, size_t len) = 0; // �������� ������ � ����. ������������ �������� - ������� ������� ���� ������� ��������
		virtual void Close(File* f) = 0; // ������� ����	
	};
}
