#include "cgiClass.h"
#include <iostream>
#include <stdlib.h>     /* getenv */
#include <string>
#include <vector>
#include<stdio.h> 
#include<string.h>
#include<stdlib.h>

using namespace std;

cgiClass::cgiClass(void)
{
	status = 0;
	error = 0;
	nextArg = 0;
	argCount = 0;
	method = METHOD_NONE;
};

int
cgiClass::showArgs(void)
{
	string arg;
	string name;
	string value;
	std::string::size_type pos;

	cout << "<ul>" << endl;

	for (std::vector<std::string>::iterator it = arguments.begin(); it != arguments.end(); ++it)
	{
		arg = *it;
		pos = arg.find('=', 0);
		name = arg.substr(0, pos);
		value = arg.substr(pos + 1);
		cout << "<li>" << name << " = " << value << "</li>" << endl;
	}

	cout << "</ul>" << endl;
	return (0);
}
#ifndef WIN32
#define WIN32
#endif

#if defined WIN32
int
cgiClass::getPostArgs(char* args)
{
	char* arg;
	char* copy = _strdup(args);

	// Loop getting key-value pairs using `strtok`
	// ...

	char* next_token = NULL;

	arg = strtok_s(copy, "&", &next_token);
	while (arg != NULL)
	{
		arguments.push_back(arg);
		arg = strtok_s(NULL, "&", &next_token);
		argCount++;
	}

	// Must be free'd since we allocated a copy above
	free(copy);
	return (0);
}
#else

int
cgiClass::getPostArgs(char* args)
{
	char* arg;
	char* copy = strdup(args);
	int count = 0;

	// Loop getting key-value pairs using `strtok`
	// ...

	arg = strtok(copy, "&");
	while (arg != NULL)
	{
		arguments.push_back(arg);
		arg = strtok(NULL, "&");
	}

	// Must be free'd since we allocated a copy above
	free(copy);
	return (0);
}
#endif
int
cgiClass::getPostData()
{
	char* data;
	char* env;
#if defined WIN32
	char buffer[1024];
	errno_t returnStatus;
#endif
	size_t length = 0;
	int rval = -1;
	std::vector<std::string> arguments;


	// fileContentLength = getenv();
#if defined WIN32
	returnStatus = getenv_s(&length, buffer, 1024, "CONTENT_LENGTH");
	env = &buffer[0];
#else
	env = getenv("CONTENT_LENGTH");
	if (env)
	{
		length = strlen(env);
	}
#endif
	//printf("    \"CONTENT_LENGTH_length\":\"%lu\",\n", length );

	if (length <= 0)
	{
		rval = -1;
	}
	else
	{
		length = atoi(env);
		//printf("    \"CONTENT_LENGTH_Data_length\":\"%lu\",\n", length );
		data = (char*)malloc(length + 1);

		if (data == NULL)
		{
			printf("    \"CONTENT_LENGTH_malloc\":\"fails\",\n");
			rval = -1;
		}
		else
		{
			memset(data, 0, length + 1);
			if (fread(data, 1, length, stdin) == 0)
			{
				//printf("    \"CONTENT_LENGTH_fread\":\"fails\",\n" );
				rval = -1;
			}
			else
			{
				rval = getPostArgs(data);
				//printf("    \"getPostArgs returns\":\"%d\",\n", rval );
			}
		}
		free(data);
	}
	return (rval);
}

int
cgiClass::getArgs(void)
{
	int rval = 0;
	char* env;

#if defined WIN32
	char buffer[1024];
	errno_t returnStatus;
#endif
	size_t length;

#if defined WIN32
	returnStatus = getenv_s(&length, buffer, 1024, "REQUEST_METHOD");
	env = &buffer[0];
#else
	env = getenv("REQUEST_METHOD");
	if (env)
	{
		length = strlen(env);
	}
#endif
	//printf("    \"REQUEST_METHOD_length\":\"%lu\",\n", length );
	//printf("    \"REQUEST_METHOD\":\"%s\",\n", env );

	if (length > 0)
	{
		if (strcmp(env, "POST") == 0)
		{
			method = METHOD_POST;
			rval = getPostData();
		}
		else if (strcmp(env, "GET") == 0)
		{
			method = METHOD_GET;
#if defined WIN32
			returnStatus = getenv_s(&length, buffer, 1024, "QUERY_STRING");
#else
			env = getenv("QUERY_STRING");
			if (env)
			{
				length = strlen(env);
			}
			else
			{
				length = 0;
			}
#endif

			if (length > 0)
			{
				rval = getPostArgs(env);
			}
			else
			{
				rval = -1;
			}
		}
		else
		{
			rval = -2;
		}
		if (rval == 0)
		{
			// Args received - Now parse to variables

			string arg;
			string name;
			string val;
			std::string::size_type pos;;
			int i = 0;
			for (std::vector<std::string>::iterator it = arguments.begin();
				it != arguments.end();
				++it, ++i)
			{
				arg = *it;
				pos = arg.find('=', 0);
				name = arg.substr(0, pos);
				val = arg.substr(pos + 1);
				argCount++;
			}
		}
	}
	if (rval == 0)
	{
		return (argCount);
	}
	return (rval);
}