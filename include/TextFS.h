#pragma once

#include "TestTask.h"

namespace TestTask {
	struct textFS : public IVFS {
		virtual File* Open(const char* name) final;
		virtual File* Create(const char* name) final;
		virtual size_t Read(File* f, char* buff, size_t len) final;
		virtual size_t Write(File* f, char* buff, size_t len) final;
		virtual void Close(File* f) final;
	};
}
