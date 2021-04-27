#pragma once

#include <vector>
#include<string>

using namespace std;

#define METHOD_NONE		0
#define METHOD_POST		1
#define METHOD_GET		2

class cgiClass
{
	int nextArg;
	int argCount;
	int getPostData(void);
	int getPostArgs(char* args);

public:
	int method; 
	std::vector<std::string>  arguments;
	int status;
	int error;

	cgiClass();
	int getArgs(void);
	int getArg(char* name);
	int showArgs(void);
};



