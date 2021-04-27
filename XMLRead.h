#pragma once
#include <string>
#include <sstream>
#include <iostream>
#include <iterator>

using namespace std;

#define XML_MAX_NAME	512
#define XML_MAX_VALUE	16536

constexpr auto  XML_TYPE_NONE = 0;
constexpr auto  XML_TYPE_ELEMENT = 1;
constexpr auto  XML_TYPE_TEXT = 2;
constexpr auto  XML_TYPE_END_ELEMENT = 3;
constexpr auto  XML_TYPE_FILE_END = 4;

enum class xmlParseState
{
	initial = 0,
	foundElement = 1,
	returnedText = 2,
	closedElement = 3
};

class XMLRead
{
private:
	int idx = 0;
	char* xml = (char *)NULL;
	enum class xmlParseState state = xmlParseState::initial;;

public:
	int type = XML_TYPE_NONE;
	int depth = -1;
	DWORD fileLength = 0;
	char name[XML_MAX_NAME] = { 0, };
	char value[XML_MAX_VALUE] = { 0, };

	XMLRead(void)
	{
		idx = 0;
		xml = (char*)NULL;
		state = xmlParseState::initial;
		type = XML_TYPE_NONE;
		name[0] = 0;
		value[0] = 0;
	};
	~XMLRead(void)
	{
		if (XMLRead::xml)
		{
			free(XMLRead::xml);
			XMLRead::xml = NULL;
		}
	};
	int getEntry(void);
	int open(const char* path);

};

