#include <iostream>
#include "TextFS.h"


int main() {

	TestTask::textFS filesys;
	TestTask::File* myfile = filesys.Create("C:/test1/test2/test3.txt");
	char message[] = "Hi there, my name is Timur ";
	filesys.Write(myfile, message, 26);
	filesys.Close(myfile);

	return 0;
}

