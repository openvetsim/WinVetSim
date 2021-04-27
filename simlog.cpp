/*
 * sim-log.c
 *
 * Handle the simmgr logging system
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSimDevelopers/sim-mgr).
 *
 * Copyright (c) 2019 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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
 *
 * The log file is created in the directory /var/www/html/simlogs
 * The filename is created from the scenario name and start time
 */
#include "vetsim.h"
/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include "../include/simmgr.h"
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
*/
#define SIMLOG_DIR		"C:/inetpub/wwwroot/simlogs"
#define SIMLOG_NAME_LENGTH 128
#define MAX_TIME_STR	24
#define MAX_LINE_LEN	512

/*
int do_chown(const char* file_path,
	const char* user_name,
	const char* group_name)
{
	uid_t          uid;
	gid_t          gid;
	struct passwd* pwd;
	struct group* grp;

	pwd = getpwnam(user_name);
	if (pwd == NULL) {
		return (-1);
	}
	uid = pwd->pw_uid;

	grp = getgrnam(group_name);
	if (grp == NULL) {
		return (-2);
	}
	gid = grp->gr_gid;

	if (chown(file_path, uid, gid) == -1) {
		return (-3);
	}
	return (0);
}
*/
/*
 * FUNCTION:
 *		simlog_create
 *
 * ARGUMENTS:
 *
 *
 * RETURNS:
 *		On success, returns 0. On fail, returns -1.
 */
char simlog_file[SIMLOG_NAME_LENGTH] = { 0, };
int simlog_initialized = 0;
FILE* simlog_fd;
int simlog_line;	// Last line written
int lock_held = 0;

int
simlog_create()
{
	char timeStr[MAX_TIME_STR];
	char msgBuf[MAX_LINE_LEN];

	// Format from status.scenario.start: "2021-02-22_09.31.53"
	strftime(timeStr, MAX_TIME_STR, "%Y-%m-%d_%H.%M.%S", &simmgr_shm->status.scenario.tmStart );
	sprintf_s(simlog_file, SIMLOG_NAME_LENGTH, "%s/%s_%s.log", SIMLOG_DIR, timeStr, simmgr_shm->status.scenario.active);

	(void)simlog_open(SIMLOG_MODE_CREATE);
	if (simlog_fd)
	{
		printf("simlog_open succeeds\n");
		simlog_line = 0;
		simmgr_shm->logfile.active = 1;
		snprintf(msgBuf, MAX_LINE_LEN, "Scenario: '%s' Date: %s", simmgr_shm->status.scenario.active, simmgr_shm->status.scenario.start);
		simlog_write((char*)"Start");
		simlog_close();
		sprintf_s(simmgr_shm->logfile.filename, FILENAME_SIZE, "%s_%s.log", timeStr, simmgr_shm->status.scenario.active);
		sprintf_s(simmgr_shm->logfile.vfilename, FILENAME_SIZE, "%s_%s.mp4", timeStr, simmgr_shm->status.scenario.active);
		return (0);
	}
	else
	{
		printf("simlog_open fails\n");
		simmgr_shm->logfile.active = 0;
		sprintf_s(simmgr_shm->logfile.filename, FILENAME_SIZE, "%s", "");
		simmgr_shm->logfile.lines_written = 0;
		return (-1);
	}
}

void
simlog_entry(char* msg)
{
	int sts;

	if (strlen(msg) == 0)
	{
		log_message("", "simlog_entry with null message");
	}
	else
	{
		if ((strlen(simlog_file) > 0) && (simmgr_shm->logfile.active))
		{
			sts = simlog_open(SIMLOG_MODE_WRITE);
			if (sts == 0)
			{
				(void)simlog_write(msg);
				(void)simlog_close();
			}
		}
	}
}

int
simlog_open(int rw)
{
	int sts;
	char buffer[512];
	char errBuffer[256];
	int trycount;

	if (simlog_fd)
	{
		log_message("", "simlog_open called with simlog_fd already set");
		printf("simlog_open called with simlog_fd already set\n");
		return (-1);
	}
	if (lock_held)
	{
		log_message("", "simlog_open called with lock_held set");
		printf("simlog_open called with lock_held set\n");
		return (-2);
	}
	if (rw == SIMLOG_MODE_WRITE || rw == SIMLOG_MODE_CREATE)
	{
		// Take the lock
		trycount = 0;
		while (trycount++ < 50)
		{
			sts = WaitForSingleObject(simmgr_shm->logfile.sema, INFINITE);
			if (sts == WAIT_OBJECT_0)
			{
				break;
			}
			else
			{
				printf("simlog_open failed to take sema 0x%08xL\n");
				return (-3);
			}
		}
		if (trycount >= 50)
		{
			// Could not get lock soon enough. Try again next time.
			log_message("", "simlog_open failed to take logfile mutex");
			printf("simlog_open failed to take logfile mutex\n");
			return (-1);
		}
		lock_held = 1;
		if (rw == SIMLOG_MODE_CREATE)
		{
			sts = fopen_s(&simlog_fd, simlog_file, "w");
		}
		else
		{
			sts = fopen_s(&simlog_fd, simlog_file, "a");
		}
		if (sts || !simlog_fd)
		{
			strerror_s(errBuffer, 256, errno);
			sprintf_s(buffer, 512, "simlog_open failed to open for write: %s : %s",
				simlog_file, errBuffer);
			log_message("", buffer);
			printf("simlog_open failed to open for write: %s : %s\n",
				simlog_file, errBuffer);
			ReleaseMutex(simmgr_shm->logfile.sema);
			lock_held = 0;
		}
	}
	else
	{
		sts = fopen_s(&simlog_fd, simlog_file, "r");
		if (sts || !simlog_fd)
		{
			sprintf_s(buffer, 512, "simlog_open failed to open for read: %s", simlog_file); 
			printf("simlog_open failed to open for read:%s : %s\n",
				simlog_file, errBuffer);
			log_message("", buffer);
		}
	}
	if (!simlog_fd)
	{
		printf("simlog_open failed\n");
		return (-1);
	}
	return (0);
}

int
simlog_write(char* msg)
{
	if (!simlog_fd)
	{
		log_message("", "simlog_write called with closed file");
		return (-1);
	}
	if (!lock_held)
	{
		log_message("", "simlog_write called without lock held");
		return (-1);
	}
	if (strlen(msg) > MAX_LINE_LEN)
	{
		log_message("", "simlog_write overlength string");
		return (-1);
	}
	if (strlen(msg) <= 0)
	{
		log_message("", "simlog_write empty string");
		return (-1);
	}
	fprintf(simlog_fd, "%s %s %s %s\n",
		simmgr_shm->status.scenario.runtimeAbsolute,
		simmgr_shm->status.scenario.runtimeScenario,
		simmgr_shm->status.scenario.runtimeScene,
		msg);

	simlog_line++;
	simmgr_shm->logfile.lines_written = simlog_line;
	return (simlog_line);
}

size_t
simlog_read(char* rbuf)
{
	sprintf_s(rbuf, MAX_LINE_LEN, "%s", "");

	if (!simlog_fd)
	{
		log_message("", "simlog_read called with closed file");
		return (-1);
	}
	fgets(rbuf, MAX_LINE_LEN, simlog_fd);
	return (strlen(rbuf));
}

size_t
simlog_read_line(char* rbuf, int lineno)
{
	int line;
	char* sts;

	sprintf_s(rbuf, MAX_LINE_LEN, "%s", "");

	if (!simlog_fd)
	{
		log_message("", "simlog_read_line called with closed file");
		return (-1);
	}

	fseek(simlog_fd, 0, SEEK_SET);
	for (line = 1; line <= lineno; line++)
	{
		sts = fgets(rbuf, MAX_LINE_LEN, simlog_fd);
		if (!sts)
		{
			return (-1);
		}
	}

	return (strlen(rbuf));
}

void
simlog_close()
{
	// Close the file
	if (simlog_fd)
	{
		fclose(simlog_fd);
		simlog_fd = NULL;
	}

	// Release the MUTEX
	if (lock_held)
	{
		ReleaseMutex(simmgr_shm->logfile.sema);
		lock_held = 0;
	}
}

void
simlog_end()
{
	int sts;

	if ((simlog_fd == NULL) && (simmgr_shm->logfile.active))
	{
		sts = simlog_open(SIMLOG_MODE_WRITE);
		if (sts == 0)
		{
			simlog_write((char*)"End");
			simlog_close();
		}
	}
	simmgr_shm->logfile.active = 0;
}
