// -*- coding: utf-8 -*-
#include <iostream>
#include "TextFS.h"
#include "MultiThreadTask.h"
#include "SingleThreadTask.h"

int main() {

	char toWrite[] = "A message\n";

	singleThreadTest(toWrite);
	multiThreadTest();

	return 0;
}
