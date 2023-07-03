// -*- coding: utf-8 -*-
#pragma once

void multiThreadTest();

void threadTask1(char toWrite, int threadNumber, char* filePath);

void threadTask2(int threadNumber, char* filePath);
