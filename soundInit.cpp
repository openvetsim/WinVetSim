/*
 * soundInit.cpp
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSimDevelopers/sim-mgr).
 *
 * Copyright (c) 2020-2023 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
 * Copyright (c) 2020-2025 ITown Design, Ithaca, NY
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
/*
 NOTE: The parsing functions (CSV file) are common with the
 soundSense.cpp file in the sim-ctl code.
*/
#include "vetsim.h"
/*
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#include <string>  
#include <cstdlib>
#include <sstream>
#include <ctime>
*/
const char soundDir[] = "/var/www/html/sim-sounds";
const char CSVfilename[] = "/var/www/html/sim-sounds/soundList.csv";
const char HTMLfilename[] = "/var/www/html/sim-sounds/soundList.php";

extern int debug;

#define SOUND_TYPE_UNUSED	0
#define SOUND_TYPE_HEART	1
#define SOUND_TYPE_LUNG		2
#define SOUND_TYPE_PULSE	3
#define SOUND_TYPE_GENERAL	4

#define SOUND_TYPE_LENGTH	8
struct soundType
{
	int type;
	char typeName[SOUND_TYPE_LENGTH];
};
struct soundType soundTypes[] =
{
	{ SOUND_TYPE_UNUSED,	"unused" },
	{ SOUND_TYPE_HEART,		"heart" },
	{ SOUND_TYPE_LUNG,		"lung" },
	{ SOUND_TYPE_PULSE,		"pulse" },
	{ SOUND_TYPE_GENERAL,	"general" }
};

int
typeNameToIndex(char* typeName)
{
	int i;

	for (i = 0; i < 5; i++)
	{
		if (strcmp(typeName, soundTypes[i].typeName) == 0)
		{
			return (soundTypes[i].type);
		}
	}
	return -1;
}

#define SOUND_NAME_LENGTH	32
#define FILE_NAME_LENGTH	256
#define MSGBUF_LENGTH	2048

struct sound
{
	int type;
	int index;
	char name[SOUND_NAME_LENGTH];
	int low_limit;
	int high_limit;
	char filename[SOUND_NAME_LENGTH];
};

int maxSounds = 0;
struct sound* soundList;
int soundIndex = 0;
char msgbuf[MSGBUF_LENGTH];

int
addSoundToList(int type, int index, const char* name, int low_limit, int high_limit)
{
	int sts;
	if (soundIndex >= maxSounds)
	{
		sprintf_s(msgbuf, "Too many tracks in sound list");
		log_message("", msgbuf);
		sts = -1;
	}
	else if (soundList[soundIndex].type == SOUND_TYPE_UNUSED)
	{
		soundList[soundIndex].type = type;
		soundList[soundIndex].index = index;
		memcpy(soundList[soundIndex].name, name, SOUND_NAME_LENGTH);
		soundList[soundIndex].low_limit = low_limit;
		soundList[soundIndex].high_limit = high_limit;
		soundIndex++;
		sts = 0;
	}
	else
	{
		sprintf_s(msgbuf, "bad entry in sound list");
		log_message("", msgbuf);
		sts = -1;
	}
	return (sts);
}


int
initSoundList(void)
{
	FILE* file;
	int lines;
	char line[1024];
	char* in;
	char* out;
	char clean[1060];

	struct sound sd;
	char typeName[SOUND_TYPE_LENGTH];
	char errstr[256];
	int sts;

	sts = fopen_s(&file, CSVfilename, "r");
	if (file == NULL)
	{
		if (debug)
		{
			perror("fopen");
			fprintf(stderr, "Failed to open %s\n", CSVfilename);
		}
		else
		{
			strerror_s(errstr, 256, errno);
			sprintf_s(msgbuf, "Failed to open %s %s", CSVfilename, errstr );
			log_message("", msgbuf);
		}
		//exit(-2);
	}

	for (lines = 0; fgets(line, 1024, file); lines++)
	{
	}
	if (debug > 1) printf("Found %d lines in file\n", lines);
	maxSounds = lines + 1;
	soundList = (struct sound*)calloc(maxSounds, sizeof(struct sound));

	fseek(file, 0, SEEK_SET);
	for (lines = 0; fgets(line, 1024, file); lines++)
	{
		in = (char*)line;
		out = (char*)clean;
		while (*in)
		{
			switch (*in)
			{
			case '\t': case ';': case ',':
				*out = ' ';
				break;
			case ' ':
				*out = '_';
				break;
			default:
				*out = *in;
				break;
			}
			out++;
			in++;
		}
		*out = 0;
		sts = sscanf_s(clean, "%s %d %s %d %d",
			typeName, (unsigned int)sizeof(typeName),
			&sd.index,
			sd.name, (unsigned int)sizeof(sd.name),
			&sd.low_limit,
			&sd.high_limit);
		if (sts == 5)
		{
			sd.type = typeNameToIndex(typeName);
			addSoundToList(sd.type, sd.index, sd.name, sd.low_limit, sd.high_limit);
		}
		else
		{
			sprintf_s(msgbuf, "sscanf returns %d for line: \"%s\"\n", sts, line);
			log_message("", msgbuf);
		}
	}
	return (0);
}

void
showSounds(void)
{
	int i;

	for (i = 0; i < maxSounds; i++)
	{
		if (soundList[i].type != SOUND_TYPE_UNUSED)
		{
			printf("%s,%d,%s,%d,%d\n",
				soundTypes[soundList[i].type].typeName, soundList[i].index, soundList[i].name, soundList[i].low_limit, soundList[i].high_limit);
		}
	}
}

#define FILE_NAME_LENGTH	256

struct soundFile
{
	int index;
	char filename[FILE_NAME_LENGTH];
};

struct soundFile* soundFileList;
int maxFiles;

void
createSoundFileList(void)
{
#if 0
	struct dirent** namelist;
	int n;
	int i;
	int index;
	char* extention;
	int next;
	n = scandir(soundDir, &namelist, NULL, alphasort);
	if (debug) printf("Max Sounds is %d File Count %d\n", maxSounds, n);
	maxFiles = n;

	soundFileList = (struct soundFile*)calloc(n, sizeof(struct soundFile));

	for (next = 0, i = 0; i < n; i++)
	{
		extention = strrchr(namelist[i]->d_name, '.');
		if (extention)
		{
			if (strncmp(extention, ".ogg", 4) == 0)
			{
				index = atoi(namelist[i]->d_name);
				if (index > 0)
				{
					if (debug) printf("Name %s  Index %d\n", namelist[i]->d_name, index);
					soundFileList[next].index = index;
					strcpy(soundFileList[next].filename, namelist[i]->d_name);
					next++;
				}
			}
		}
	}
#endif
}

struct soundFile*
	getSoundFileEntry(int index)
{
	int i;

	for (i = 0; i < maxFiles; i++)
	{
		if (soundFileList[i].index == index)
		{
			return (&soundFileList[i]);
		}
	}
	return (NULL);
}

void
createSoundFile(void)
{
	int i;
	FILE* fd;
	int sts;

	sts = fopen_s(&fd, HTMLfilename, "w");
	if (fd)
	{
		fprintf(fd, "<script>\nvar soundPlayList = {\n");
		for (i = 0; i < maxSounds; i++)
		{
			fprintf(fd, "%d : { type:'%s', index:%d,name:'%s',low_limit:%d,high_limit:%d },\n",
				i,
				soundTypes[soundList[i].type].typeName,
				soundList[i].index,
				soundList[i].name,
				soundList[i].low_limit,
				soundList[i].high_limit);
		}
		fprintf(fd, "};\n</script>\n\n");
		fprintf(fd, "<style>\n.soundList\n{\n\tdisplay: none;\n}\n</style>\n");
		fprintf(fd, "<div class='soundList'>\n");

		for (i = 0; i < maxFiles; i++)
		{
			if (debug > 1) printf("%03d: %s\n", soundFileList[i].index, soundFileList[i].filename);
			if (soundFileList[i].index > 0)
			{
				fprintf(fd, "    <audio id=\"snd_%d\" preload ><source src=\"/sim-sounds/%s\" type=\"audio/ogg\"></audio>\n",
					soundFileList[i].index, soundFileList[i].filename);
			}
		}
		fprintf(fd, "</div>\n");
		fclose(fd);
	}
	else
	{
		perror("fopen");
	}
}
int
checkSoundFileStatus(void)
{
	// See if the CSV is Newer than the HTML file
	struct stat CSVsb;
	struct stat HTMLsb;
	struct tm CSVtime;
	//struct tm HTMLtime;
	char ctime[64];
	//char htime[64];
	int sts;
	size_t charsBack;
	const char format[] = "%c";

	sts = stat(CSVfilename, &CSVsb);
	if (sts < 0)
	{
		perror("stat for CSVfile");
		exit(EXIT_FAILURE);
	}
	sts = localtime_s(&CSVtime, &CSVsb.st_mtime);
	charsBack = strftime(ctime, 64, format, &CSVtime);
	if (charsBack == 0)
	{
		printf("strftime for CSV fails\n");
		exit(-1);
	}
	if (debug > 0) printf("CSV File:\n\tModified %s\n", ctime);

	sts = stat(HTMLfilename, &HTMLsb);
	if (sts < 0)
	{
		perror("stat for HTMLfile");
	}
	else
	{
/*
		sts = localtime(&HTMLtime, &HTMLsb.st_mtime);
		charsBack = strftime(htime, 64, format, HTMLtime);
		if (charsBack == 0)
		{
			printf("strftime for HTML fails\n");
			exit(-1);
		}
		if (debug > 0) printf("HTML File:\n\tModified %s\n", htime);
		if (HTMLsb.st_mtime > CSVsb.st_mtime)
		{
			return (1);
		}
*/
	}
	return (-1);
}

int soundInit(void)
{
	int sts;

	sts = checkSoundFileStatus();
	if (sts != 0)
	{
		initSoundList();
		if (debug > 1)
		{
			printf("Show Sounds:\n");
			showSounds();
		}
		createSoundFileList();
		createSoundFile();
	}
	return (sts);
}