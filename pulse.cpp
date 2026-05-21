/*
 * simpulse.cpp
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
 * Time clock for the Cardiac and Respiratory systems. This application monitors the shared
 * memory to get the rate parameters and issues sync signals to the various systems.
 *
 * This process runs independently from the SimMgr. It has two timers; one for the heart rate (pulse) and
 * one for the breath rate (respiration). It runs as two threads. The primary thread listens for connections
 * from clients, and the child thread monitors the pulse and breath counts to send sync messages to the
 * clients.
 *
 * Listen for a connections on Port 50200 (SimMgr Event Port)
 *
 * 		1 - On connection, the daemon will fork a task to support the connection
 *		2 - Each connection waits on sync messages
 *
 * Copyright (C) 2016-2018 Terence Kelleher. All rights reserved.
 *
 */

#include "vetsim.h"

using namespace std;

extern struct simmgr_shm shmSpace;

/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <iostream>
#include <vector>  
#include <string>  
#include <cstdlib>
#include <sstream>

#include <ctime>
#include <math.h>       // 
#include <netinet/in.h>
#include <netinet/ip.h> 

#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
*/
// #define DEBUG
#define BUF_SIZE 2048
char p_msg[BUF_SIZE];

int quit_flag = 0;

int currentPulseRate = 0;
int currentVpcFreq = 0;

int currentBreathRate = 0;
unsigned int lastManualBreath = 0;

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")


void getControllerVersion(int index );

void set_pulse_rate(int bpm);
void set_breath_rate(int bpm);
void calculateVPCFreq(void);
void sendStatusPort(int listener);

/* struct to hold data to be passed to a thread
   this shows how multiple data items can be passed to a thread */
struct listener
{
	int allocated;
	int thread_no;
	SOCKET cfd;
	char ipAddr[32];
	char version[32];
};
#define MAX_LISTENERS 10

struct listener listeners[MAX_LISTENERS];

char pulseWord[] = "pulse\n";
char pulseWordVPC[] = "pulseVPC\n";
char breathWord[] = "breath\n";

#define VPC_ARRAY_LEN	200
int vpcFrequencyArray[VPC_ARRAY_LEN];
int vpcFrequencyIndex = 0;
int vpcType = 0;
int afibActive = 0;
#define IS_CARDIAC	1
#define NOT_CARDIAC	0

void pulseTimer(void);
void pulseBroadcastLoop(void);

std::mutex breathSema;
std::mutex pulseSema;

int beatPhase = 0;
int vpcState = 0;
int vpcCount = 0;
ULONGLONG nextBreathTime = 0;
ULONGLONG nextPulseTime = 0;
ULONGLONG breathInterval = 0;
ULONGLONG pulseInterval = 0;

void
resetVpc(void)
{
	beatPhase = 0;
	vpcState = 0;
	vpcCount = 0;
}

/* vpcState is set at the beginning of a sinus cycle where VPCs will follow.
	vpcState is set to the number of VPCs to be injected.

	beatPhase is set to the number of beat ticks to wait for the next event. This is typically:
		From Sinus to Sinus:	10
		From Sinus to VPC1:		7
		From VPC1 to Sinus:		13
		From VPC1 to VPC2:		7
		From VPC2 to Sinus:		16
		From VPC2 to VPC3:		7
		From VPC3 to Sinus:		19
*/
extern void setPulseState(int);
extern void hrLogBeat(void);

static void
pulse_beat_handler(void)
{
	pulseSema.lock();
	if (currentPulseRate > 0)
	{
		if ((vpcType > 0) || (afibActive))
		{
			if (beatPhase-- <= 0)
			{
				if (vpcState > 0)
				{
					// VPC Injection
					simmgr_shm->status.cardiac.pulseCountVpc++;
					hrLogBeat();
					vpcState--;
					switch (vpcState)
					{
					case 0: // Last VPC
						switch (simmgr_shm->status.cardiac.vpc_count)
						{
						case 0:	// This should only occur if VPCs were just disabled.
						case 1:
						default:	// Should not happen
							beatPhase = 13;
							break;
						case 2:
							beatPhase = 16;
							break;
						case 3:
							beatPhase = 19;
							break;
						}
						break;
					default:
						beatPhase = 6;
						break;
					}
				}
				else
				{
					// Normal Cycle
					simmgr_shm->status.cardiac.pulseCount++;
					hrLogBeat();
					if (afibActive)
					{
						// Next beat phase is between 50% and 200% of standard. 
						// Calculate a random from 0 to 14 and add to 5
						beatPhase = 5 + (rand() % 14);
					}
					else if ((vpcType > 0) && (currentVpcFreq > 0))
					{
						vpcFrequencyIndex++;
						if ( ! (vpcFrequencyIndex < VPC_ARRAY_LEN ))
						{
							vpcFrequencyIndex = 0;
						}
						if (vpcFrequencyArray[vpcFrequencyIndex] > 0)
						{
							vpcState = simmgr_shm->status.cardiac.vpc_count;
							beatPhase = 6;
						}
						else
						{
							beatPhase = 9;
						}
					}
					else
					{
						beatPhase = 9;	// Preset for "normal"
					}
				}
			}
		}
		else
		{
			simmgr_shm->status.cardiac.pulseCount++;
			hrLogBeat();
			setPulseState(2);
		}
	}
	pulseSema.unlock();
}
static void
breath_beat_handler(void)
{
	breathSema.lock();
	if (simmgr_shm->status.respiration.rate > 0)
	{
		simmgr_shm->status.respiration.breathCount++;
	}
	breathSema.unlock();
}

void
calculateVPCFreq(void)
{
	int count = 0;
	int i;
	int val;

	if (simmgr_shm->status.cardiac.vpc_freq == 0)
	{
		currentVpcFreq = 0;
	}
	else
	{
		// get 100 samples for 100 cycles of sinus rhythm between 10 and 90
		for (i = 0; i < VPC_ARRAY_LEN; i++)
		{
			val = rand() % 100;
			if (val > currentVpcFreq)
			{
				vpcFrequencyArray[i] = 0;
			}
			else
			{
				vpcFrequencyArray[i] = 1;
				count++;
			}
		}
#ifdef DEBUG
		sprintf_s(p_msg, "calculateVPCFreq: request %d: result %d", currentVpcFreq, count);
		log_message("", p_msg);
#endif
		vpcFrequencyIndex = 0;
	}
}
/*
 * FUNCTION:
 *		getWaitTimeMsec
 *
 * ARGUMENTS:
 *		rate	- Rate in Beats per minute
 *		isCaridac	- Set to IS_CARDIAC for the cardiac timer
 *		isFib		- Set if 10 phase timer is needed
 *
 * DESCRIPTION:
 *		Calculate and set the timer, used for both heart and breath.
 *
 * ASSUMPTIONS:
 *		Called with pulseSema or breathSema held
*/
ULONGLONG
getWaitTimeMsec(int rate, int isCardiac, int isFib)
{
	double frate;	// Beats per minute
	double sec_per_beat;
	double msec_per_beat_f;
	ULONGLONG wait_time_msec;

	frate = (double)rate;
	sec_per_beat = 1 / (frate / 60);

	// Note that the heart beat handler is called 10 times per interval, 
	// to provide VPC and AFIB functions
	if (isFib)
	{
		sec_per_beat = sec_per_beat / 10;
	}
	msec_per_beat_f = sec_per_beat * 1000;
	wait_time_msec = (ULONGLONG)(msec_per_beat_f);
	return (wait_time_msec);
}
/*
 * FUNCTION:
 *		resetTimer
 *
 * ARGUMENTS:
 *		rate	- Rate in Beats per minute
 *		isCaridac	- Set to IS_CARDIAC for the cardiac timer
 *		isFib		- Set if 10 phase timer is needed
 *
 * DESCRIPTION:
 *		Calculate and set the timer, used for both heart and breath.
 *
 * ASSUMPTIONS:
 *		Called with pulseSema or breathSema held
*/
void
resetTimer(int rate, int isCardiac, int isFib)
{
	ULONGLONG wait_time_msec;
	ULONGLONG remaining;
	ULONGLONG now = simmgr_shm->server.msec_time;

	wait_time_msec = getWaitTimeMsec(rate, isCardiac, isFib);

	//printf("Set Timer: Rate %d sec_per_beat %f %llu\n", rate, sec_per_beat, wait_time_msec);
	if (isCardiac)
	{
		pulseInterval = wait_time_msec;
		remaining = nextPulseTime - now;
		if (remaining > (now + pulseInterval))
		{
			nextPulseTime = now + wait_time_msec;
		}
		
	}
	else
	{
		breathInterval = wait_time_msec;
		remaining = nextBreathTime - now;
		if (remaining > (now + pulseInterval))
		{
			nextBreathTime = now + wait_time_msec;
		}
	}
}

/*
 * FUNCTION:
 *		set_pulse_rate
 *
 * ARGUMENTS:
 *		bpm	- Rate in Beats per Minute
 *
 * DESCRIPTION:
 *		Calculate and set the wait time in usec for the beats.
 *		The beat timer runs at 10x the heart rate
 *
 * ASSUMPTIONS:
 *		Called with pulseSema held
*/

void
set_pulse_rate(int bpm)
{
	// When the BPM is zero, we set the timer based on 60, to allow it to continue running.
	// No beats are sent when this occurs, but the timer still runs.
	if (bpm == 0)
	{
		bpm = 60;
	}
	if ((vpcType > 0) || (afibActive))
	{
		resetTimer(bpm, IS_CARDIAC, 1 );
	}
	else
	{
		resetTimer(bpm, IS_CARDIAC, 0);
	}
}

// restart_breath_timer is called when a manual respiration is flagged. 
void
restart_breath_timer(void)
{
	ULONGLONG now = simmgr_shm->server.msec_time;
	ULONGLONG wait_time_msec;

	wait_time_msec = getWaitTimeMsec(simmgr_shm->status.respiration.rate, 0, 0);
	breathInterval = wait_time_msec;
	
	// For very slow cycles (less than 15 BPM), set initial timer to half the cycle plus add 0.1 seconds.
	if (simmgr_shm->status.respiration.rate < 15)
	{
		nextBreathTime = now + ((breathInterval / 2) + 100);
	}
	else
	{
		nextBreathTime = now + breathInterval;
	}
}

void
set_breath_rate(int bpm)
{
	if (bpm == 0)
	{
		bpm = 60;
	}

	resetTimer(bpm, NOT_CARDIAC, 0 );
}
HANDLE pusleTimerH;
HANDLE bcastTimerH;
SECURITY_DESCRIPTOR timerSecDesc;

_SECURITY_ATTRIBUTES timerSecAttr
{
	sizeof(_SECURITY_ATTRIBUTES),

};

int
pulseTask(void )
{
	int portno = PORT_PULSE;
	int i;
	int error;
	char* sesid = NULL;
	SOCKET sfd;
	SOCKET cfd;
	struct sockaddr client_addr;
	int socklen;
	WSADATA w;
	int found;
	printf("Pulse is on port %d\n", portno);

	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		DWORD dwError;
		dwError = GetLastError();
		_tprintf(TEXT("Failed to enter background mode (%d)\n"), dwError);
	}

	DWORD dwThreadPri;
	dwThreadPri = GetThreadPriority(GetCurrentThread());
	_tprintf(TEXT("pulseTask: Current thread priority is 0x%x\n"), dwThreadPri);


	// Seed rand, needed for vpc array generation
	srand((unsigned)time(NULL));

	currentPulseRate = simmgr_shm->status.cardiac.rate;
	pulseSema.lock();
	set_pulse_rate(currentPulseRate);
	pulseSema.unlock();
	simmgr_shm->status.cardiac.pulseCount = 0;
	simmgr_shm->status.cardiac.pulseCountVpc = 0;

	currentBreathRate = simmgr_shm->status.respiration.rate;
	breathSema.lock();
	set_breath_rate(currentBreathRate);
	breathSema.unlock();
	simmgr_shm->status.respiration.breathCount = 0;

	//printf("Pulse Interval %llu Next %llu now %llu\n", pulseInterval, nextPulseTime, simmgr_shm->server.msec_time );
	//printf("Calling start_task for pulseProcessChild\n");
	(void)start_task("pulseProcessChild", pulseProcessChild);
	(void)start_task("pulseTimer", pulseTimer);
	(void)start_task("pulseBroadcastLoop", pulseBroadcastLoop);
	
	for (i = 0; i < MAX_LISTENERS; i++)
	{
		listeners[i].allocated = 0;
		simmgr_shm->simControllers[i].allocated = 0;
	}

	error = WSAStartup(0x0202, &w);  // Fill in WSA info
	if (error)
	{
		cout << "WSAStartup fails: " << GetLastErrorAsString();
		return false;                     //For some reason we couldn't start Winsock
	}
	if (w.wVersion != 0x0202)             //Wrong Winsock version?
	{
		WSACleanup();
		ios::fmtflags f(cout.flags());
		cout << "WSAStartup Bad Version: " << hex << w.wVersion;
		cout.flags(f);
		return false;
	}

	SOCKADDR_IN addr;                     // The address structure for a TCP socket

	addr.sin_family = AF_INET;            // Address family
	addr.sin_port = htons(portno);       // Assign port to this socket

    //Accept a connection from any IP using INADDR_ANY
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create socket

	if (sfd == INVALID_SOCKET)
	{
		cout << "pulseProcess - socket(): INVALID_SOCKET " << GetLastErrorAsString();
		return false;                     //Don't continue if we couldn't create a //socket!!
	}

	int enableKeepAlive = 1;
	setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&enableKeepAlive, sizeof(enableKeepAlive));

	if ( ::bind(sfd, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR )
	{
		//We couldn't bind (this will happen if you try to bind to the same  
		//socket more than once)
		cout << "pulseProcess - bind(): SOCKET_ERROR " << GetLastErrorAsString();
		return false;
	}

	int listen_result = listen(sfd, SOMAXCONN);
	if (listen_result == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(sfd);
		WSACleanup();
		return false;
	}
	socklen = sizeof(struct sockaddr_in);

	while (1)
	{
		cfd = accept(sfd, (struct sockaddr*)&client_addr, &socklen);
		if (cfd >= 0)
		{
			char newIpAddr[STR_SIZE];
			sprintf_s(newIpAddr, STR_SIZE, "%d.%d.%d.%d",
				client_addr.sa_data[2] & 0xff,
				client_addr.sa_data[3] & 0xff,
				client_addr.sa_data[4] & 0xff,
				client_addr.sa_data[5] & 0xff
			);
#if 0
			// Change to restrict to one controller only
			if (listeners[0].allocated == 1 )
			{
				printf("Closing Controller Socket\n");
				closesocket(listeners[i].cfd);
			}
			listeners[0].allocated = 1;
			listeners[0].cfd = cfd;
			listeners[0].thread_no = i;
			simmgr_shm->simControllers[0].allocated = 1;
			sprintf_s(simmgr_shm->simControllers[0].ipAddr, STR_SIZE, "%d.%d.%d.%d",
				client_addr.sa_data[2] & 0xff,
				client_addr.sa_data[3] & 0xff,
				client_addr.sa_data[4] & 0xff,
				client_addr.sa_data[5] & 0xff
			);
			printf("Connecting Controller %d.%d.%d.%d\n",
				client_addr.sa_data[2] & 0xff,
				client_addr.sa_data[3] & 0xff,
				client_addr.sa_data[4] & 0xff,
				client_addr.sa_data[5] & 0xff
			);
			// Send the Status Port Number to the listener
			sendStatusPort(i);
			printf("Send Status Port complete\n");
			found = 1;
#else
			// Check for reopen from an existing controller
			found = 0;
			for (i = 0; i < MAX_LISTENERS; i++)
			{
				if (listeners[i].allocated == 1 && strcmp(newIpAddr, simmgr_shm->simControllers[i].ipAddr) == 0)
				{
					closesocket(listeners[i].cfd);
					listeners[i].cfd = cfd;
					found = 1;
					printf("ReOpened: %s\n", newIpAddr);
					// Send the Status Port Number to the listener
					sendStatusPort(i);

					break;
				}
			}
			if (found == 0)
			{
				for (i = 0; i < MAX_LISTENERS && found == 0 ; i++  )
				{
					if (listeners[i].allocated == 0)
					{
						listeners[i].allocated = 1;
						listeners[i].cfd = cfd;
						listeners[i].thread_no = i;
						simmgr_shm->simControllers[i].allocated = 1;
						sprintf_s(simmgr_shm->simControllers[i].ipAddr, STR_SIZE, "%d.%d.%d.%d",
							client_addr.sa_data[2] & 0xff,
							client_addr.sa_data[3] & 0xff,
							client_addr.sa_data[4] & 0xff,
							client_addr.sa_data[5] & 0xff
						);
						printf("%d.%d.%d.%d\n",
							client_addr.sa_data[2] & 0xff,
							client_addr.sa_data[3] & 0xff,
							client_addr.sa_data[4] & 0xff,
							client_addr.sa_data[5] & 0xff
						);
						// Send the Status Port Number to the listener
						sendStatusPort(i);
						getControllerVersion(i);
						
						found = 1;
					}
				}
			}
			if (found == 0)
			{
				// Unable to allocate
				closesocket(cfd);
			}
#endif
		}
	}
	sprintf_s(p_msg, BUF_SIZE, "simpulse terminates");
	log_message("", p_msg);
	exit(222);
}

/*
 * FUNCTION: sendStatusPort
 *
 * ARGUMENTS:
 *		listener - Index of listener
 *
 * RETURNS:
 *		Never
 *
 * DESCRIPTION:
 *		Send the port number to the indicated listener.
*/
void
sendStatusPort(int listener)
{
	SOCKET fd;
	int len;
	char pbuf[64];

	sprintf_s(pbuf, "statusPort:%d", PORT_STATUS);
	len = (int)strlen(pbuf);

	if (listeners[listener].allocated == 1)
	{
		fd = listeners[listener].cfd;
		len = send(fd, pbuf, len, 0);
	}
}

/*
 * FUNCTION: broadcast_word
 *
 * ARGUMENTS:
 *		ptr - Unused
 *
 * RETURNS:
 *		Never
 *
 * DESCRIPTION:
 *		This process monitors the pulse and breath counts. When incremented (by the beat_handler)
 *		a message is sent to the listeners.
*/
int
broadcast_word(char* word)
{
	int count = 0;
	SOCKET fd;
	size_t len;
	int i;

	for (i = 0; i < MAX_LISTENERS; i++)
	{
		if (listeners[i].allocated == 1)
		{
			fd = listeners[i].cfd;
			len = strlen(word);
			//printf("Send %s (%d) to %d - ", word, len, i);
			len = send(fd, word, (int)len, 0);
			//printf("%d\n", len);
			if (len < 0) // This detects closed or disconnected listeners.
			{
				printf("Close listener %d\n", i);
				closesocket(fd);
				listeners[i].allocated = 0;
			}
			else
			{
				count++;
			}
		}
	}
	return (count);
}

/*
 * FUNCTION: process_child
 *
 * ARGUMENTS:
 *		ptr - Unused
 *
 * RETURNS:
 *		Never
 *
 * DESCRIPTION:
 *		This process monitors the pulse and breath counts. When incremented (by the beat_handler)
 *		a message is sent to the listeners.
 *		It also monitors the rates and adjusts the timeout for the beat_handler when a rate is changed.
*/
void
pulseTimer(void)
{
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		DWORD dwError;
		dwError = GetLastError();
		_tprintf(TEXT("Failed to elevate priority (%d)\n"), dwError);
	}
	DWORD dwThreadPri;
	dwThreadPri = GetThreadPriority(GetCurrentThread());
	_tprintf(TEXT("pulseTimer: Current thread priority is 0x%x\n"), dwThreadPri);

	ULONGLONG now;
	ULONGLONG now2;
	while (1)
	{
		Sleep(1);
		now = simmgr_shm->server.msec_time;
		if (nextPulseTime <= now)
		{
			pulse_beat_handler();
			nextPulseTime += pulseInterval;
			now2 = simmgr_shm->server.msec_time;
			if (nextPulseTime <= (now2+1))
			{
				nextPulseTime = now2;
			}
		}
		now = simmgr_shm->server.msec_time;
		if (nextBreathTime <= now)
		{
			breath_beat_handler();
			nextBreathTime += breathInterval;
			now2 = simmgr_shm->server.msec_time;
			if (nextBreathTime <= (now2+1))
			{
				nextBreathTime = now2 + breathInterval;
			}
		}
	}
	printf("pulseTimer Exit\n");
	exit(205);
}
void
pulseBroadcastLoop(void)
{
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		DWORD dwError;
		dwError = GetLastError();
		_tprintf(TEXT("Failed to elevate priority (%d)\n"), dwError);
	}
	DWORD dwThreadPri;
	dwThreadPri = GetThreadPriority(GetCurrentThread()); 
	_tprintf(TEXT("pulseBroadcastLoop: Current thread priority is 0x%x\n"), dwThreadPri);

	int count;
	int portUpdateLoops = 0;
	char pbuf[64];
	unsigned int last_pulse = simmgr_shm->status.cardiac.pulseCount;
	unsigned int last_pulseVpc = simmgr_shm->status.cardiac.pulseCountVpc;
	unsigned int last_breath = simmgr_shm->status.respiration.breathCount;
	unsigned int last_manual_breath = simmgr_shm->status.respiration.manual_count;

	while (1)
	{
		Sleep(10);
		
		if (portUpdateLoops++ > 500)
		{
			sprintf_s(pbuf, "statusPort:%d", PORT_STATUS);
			broadcast_word(pbuf);
			portUpdateLoops = 0;
		}
		
		if (last_pulse != simmgr_shm->status.cardiac.pulseCount)
		{
			last_pulse = simmgr_shm->status.cardiac.pulseCount;
			count = broadcast_word(pulseWord);
			if (count)
			{
#ifdef DEBUG
				//printf("Pulse sent to %d listeners\n", count);
#endif
			}
		}
		if (last_pulseVpc != simmgr_shm->status.cardiac.pulseCountVpc)
		{
			last_pulseVpc = simmgr_shm->status.cardiac.pulseCountVpc;
			count = broadcast_word(pulseWordVPC);
			if (count)
			{
#ifdef DEBUG
				//printf("PulseVPC sent to %d listeners\n", count);
#endif
			}
		}
		if (last_manual_breath != simmgr_shm->status.respiration.manual_count)
		{
			last_manual_breath = simmgr_shm->status.respiration.manual_count;
			simmgr_shm->status.respiration.breathCount++;
		}
		if (last_breath != simmgr_shm->status.respiration.breathCount)
		{
			last_breath = simmgr_shm->status.respiration.breathCount;
			count = 0;
			if (last_manual_breath != simmgr_shm->status.respiration.manual_count)
			{
				last_manual_breath = simmgr_shm->status.respiration.manual_count;
			}
			count = broadcast_word(breathWord);
#ifdef DEBUG
			if (count)
			{
				//printf("Breath sent to %d listeners\n", count);
			}
#endif
		}
	}
	printf("pulseBroadcastLoop exit\n");
	exit(206);
}
void
pulseProcessChild(void)
{
	int checkCount = 0;

	while (1)
	{
		Sleep(50);		// 50 msec wait

		if (strcmp(simmgr_shm->status.scenario.state, "Running") == 0)
		{
			// A place for code to run only when a scenario is active
		}
		else
		{
			
		}
		
		if (currentPulseRate != simmgr_shm->status.cardiac.rate)
		{
			pulseSema.lock();
			set_pulse_rate(simmgr_shm->status.cardiac.rate);
			currentPulseRate = simmgr_shm->status.cardiac.rate;
			pulseSema.unlock();
#ifdef DEBUG
			sprintf_s(p_msg, "Set Pulse to %d", currentPulseRate);
			log_message("", p_msg);
#endif
		}
		if (currentVpcFreq != simmgr_shm->status.cardiac.vpc_freq ||
				vpcType != simmgr_shm->status.cardiac.vpc_type)
		{
			currentVpcFreq = simmgr_shm->status.cardiac.vpc_freq;
			vpcType = simmgr_shm->status.cardiac.vpc_type;
			calculateVPCFreq();
			set_pulse_rate(simmgr_shm->status.cardiac.rate);

		}

		if (strncmp(simmgr_shm->status.cardiac.rhythm, "afib", 4) == 0 &&
			! afibActive )
		{
			afibActive = 1;
			set_pulse_rate(simmgr_shm->status.cardiac.rate);
		}
		else if (afibActive )
		{
			afibActive = 0;
			set_pulse_rate(simmgr_shm->status.cardiac.rate);

		}
		
		if (lastManualBreath != simmgr_shm->status.respiration.manual_count)
		{
			// Manual Breath has started. Reset timer to run based on this breath
			lastManualBreath = simmgr_shm->status.respiration.manual_count;
			breathSema.lock();
			restart_breath_timer();
			breathSema.unlock();
		}
		
		// If the breath rate has changed, then reset the timer
		if (currentBreathRate != simmgr_shm->status.respiration.rate)
		{
			breathSema.lock();
			set_breath_rate(simmgr_shm->status.respiration.rate);
			currentBreathRate = simmgr_shm->status.respiration.rate;
			breathSema.unlock();

			// awRR Calculation - TBD - Need real calculations
			//simmgr_shm->status.respiration.awRR = simmgr_shm->status.respiration.rate;
#ifdef DEBUG
			sprintf_s(p_msg, "Set Breath to %d", currentBreathRate);
			log_message("", p_msg);
#endif
		}
	}
	printf("pulseProcessChild Exit");
	exit(204);
}

#include <curl/curl.h>
#include <string>

// Callback for libcurl to write received data into a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t totalSize = size * nmemb;
	std::string* str = (std::string*)userp;
	str->append((char*)contents, totalSize);
	std::cout << "ctlstatus.cgi returns " << size << std::endl << contents << std::endl;

	return totalSize;
}

// Reads a web page using libcurl and returns the contents as a std::string
std::string ReadWebPage(const std::string& url)
{
	CURL* curl = curl_easy_init();
	std::string response;

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK)
		{
			// http://192.168.1.136/cgi-bin/ctlstatus.cgi
			// Optionally handle error
			std::cout << "CURL returns " << curl_easy_strerror(res) << std::endl;
			std::cout << "CURL URL " << url << std::endl;
			return "";
		}
	}
	// std::cout << "ReadWebPage " << std::endl << response << std::endl;
	return response;
}

#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

/*
 * Try to parse a full JSON text and extract the "simCtlVersion" value.
 * If the value is an object/array it will be serialized to a string.
 * Returns true if extraction succeeded and `out` contains the value.
 */
static bool try_parse_full_json_for_simCtlVersion(const std::string& text, std::string& out)
{
	try {
		auto j = json::parse(text);
		if (j.contains("simCtlVersion")) {
			if (j["simCtlVersion"].is_string())
				out = j["simCtlVersion"].get<std::string>();
			else
				out = j["simCtlVersion"].dump();
			return true;
		}
	}
	catch (...) {
		// not a pure JSON document
	}
	return false;
}

/*
 * Locate the token "simCtlVersion" inside an arbitrary page (HTML or JS),
 * extract the JSON/value that follows the ':' and return it as a string.
 * Handles values that are:
 *   - JSON object/array (starts with '{' or '[') -> returns the serialized JSON substring
 *   - JSON/string literal (starts with '"') -> returns the string content unquoted
 *   - bare token/number -> returns the token text
 */
static bool extract_simCtlVersion_from_mixed_text(const std::string& page, std::string& out)
{
	const char* token = "simCtlVersion";
	auto pos = page.find(token);
	if (pos == std::string::npos) return false;

	// find the ':' after the token
	pos = page.find(':', pos + strlen(token));
	if (pos == std::string::npos) return false;

	// advance to first non-space
	pos++;
	while (pos < page.size() && isspace((unsigned char)page[pos])) pos++;
	if (pos >= page.size()) return false;

	char c = page[pos];
	if (c == '{' || c == '[') {
		// extract balanced JSON block
		char open = c;
		char close = (open == '{') ? '}' : ']';
		size_t i = pos;
		int depth = 0;
		for (; i < page.size(); ++i) {
			if (page[i] == open) depth++;
			else if (page[i] == close) {
				depth--;
				if (depth == 0) {
					out = page.substr(pos, i - pos + 1);
					// validate by parsing
					try {
						auto j = json::parse(out);
						// if j is object/array and contains simCtlVersion nested, try to extract
						if (j.is_object() && j.contains("simCtlVersion")) {
							if (j["simCtlVersion"].is_string())
								out = j["simCtlVersion"].get<std::string>();
							else
								out = j["simCtlVersion"].dump();
						}
					}
					catch (...) {
						// accept raw substring anyway
					}
					return true;
				}
			}
			// skip over string literals to avoid matching braces inside quotes
			else if (page[i] == '"') {
				// skip quoted string
				i++;
				for (; i < page.size(); ++i) {
					if (page[i] == '\\') { i++; continue; }
					if (page[i] == '"') break;
				}
			}
		}
		return false;
	}
	else if (c == '"') {
		// quoted string
		size_t i = pos + 1;
		std::string tmp;
		for (; i < page.size(); ++i) {
			if (page[i] == '\\' && i + 1 < page.size()) {
				// handle simple escape sequences
				i++;
				switch (page[i]) {
				case 'n': tmp.push_back('\n'); break;
				case 'r': tmp.push_back('\r'); break;
				case 't': tmp.push_back('\t'); break;
				case '\\': tmp.push_back('\\'); break;
				case '"': tmp.push_back('"'); break;
				default: tmp.push_back(page[i]); break;
				}
			}
			else if (page[i] == '"') {
				out = tmp;
				return true;
			}
			else {
				tmp.push_back(page[i]);
			}
		}
		return false;
	}
	else {
		// bare token or number: read until comma, semicolon, newline or non-token char
		size_t i = pos;
		for (; i < page.size(); ++i) {
			char cc = page[i];
			if (cc == ',' || cc == ';' || cc == '\n' || cc == '\r' || cc == '<' || cc == '}' || cc == ']') break;
		}
		if (i > pos) {
			out = page.substr(pos, i - pos);
			// trim trailing spaces
			out.erase(out.find_last_not_of(" \t\r\n") + 1);
			// trim leading spaces
			out.erase(0, out.find_first_not_of(" \t\r\n"));
			// remove optional quotes
			if (!out.empty() && out.front() == '"' && out.back() == '"') {
				out = out.substr(1, out.size() - 2);
			}
			return !out.empty();
		}
		return false;
	}
}


static bool extract_simCtlVersion(const std::string& page, std::string& out)
{
	// Strategy 1: page is pure JSON
	if (try_parse_full_json_for_simCtlVersion(page, out)) return true;

	// Strategy 2: page contains an embedded JSON snippet or JS variable
	if (extract_simCtlVersion_from_mixed_text(page, out)) return true;

	// Strategy 3: try to locate a JSON substring that contains simCtlVersion key explicitly
	auto keyPos = page.find("\"simCtlVersion\"");
	if (keyPos != std::string::npos) {
		// try to find open brace before key and parse object
		auto bracePos = page.rfind('{', keyPos);
		if (bracePos != std::string::npos) {
			// attempt to find matching closing brace
			size_t i = bracePos;
			int depth = 0;
			for (; i < page.size(); ++i) {
				if (page[i] == '{') depth++;
				else if (page[i] == '}') {
					depth--;
					if (depth == 0) {
						std::string sub = page.substr(bracePos, i - bracePos + 1);
						try {
							auto j = json::parse(sub);
							if (j.contains("simCtlVersion")) {
								if (j["simCtlVersion"].is_string()) out = j["simCtlVersion"].get<std::string>();
								else out = j["simCtlVersion"].dump();
								return true;
							}
						} catch (...) { }
						break;
					}
				}
			}
		}
	}

	return false;
}

void getControllerVersion(int index) {
	if (listeners[index].allocated == 1)
	{
		char url[128];
		sprintf_s(url, "http://%s/%s", simmgr_shm->simControllers[index].ipAddr, "cgi-bin/ctlstatus.cgi");
		std::string page = ReadWebPage(url);
		std::string version;
		bool sts = extract_simCtlVersion(page, version);
		if ( sts )
		{
			// Store the version string
			strncpy_s(simmgr_shm->simControllers[index].version, STR_SIZE, version.c_str(), STR_SIZE - 1);
			cout << "Controller " << index << " simCtlVersion extraction " << ": " << version << std::endl;
		}
		else
		{
			cout << "Controller " << index << " simCtlVersion extraction " << "failed" << std::endl;
		}

	}
}
