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
};
#define MAX_LISTENERS 20

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
static void
pulse_beat_handler(void)
{
	//pulseSema.lock();
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
					if (afibActive)
					{
						// Next beat phase is between 50% and 200% of standard. 
						// Calculate a random from 0 to 14 and add to 5
						beatPhase = 5 + (rand() % 14);
					}
					else if ((vpcType > 0) && (currentVpcFreq > 0))
					{
						if (vpcFrequencyIndex++ >= VPC_ARRAY_LEN)
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
		}
	}
	//pulseSema.unlock();
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
		sprintf(p_msg, "calculateVPCFreq: request %d: result %d", currentVpcFreq, count);
		log_message("", p_msg);
#endif
		vpcFrequencyIndex = 0;
	}
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
	double frate;	// Beats per minute
	double sec_per_beat;
	double msec_per_beat_f;
	ULONGLONG now = simmgr_shm->server.msec_time;

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
	
	breathInterval = (ULONGLONG)simmgr_shm->status.respiration.rate * 60000;
	
	// For very slow cycles (less than 15 BPM), set initial timer to half the cycle plus add 1 seconds.
	if (simmgr_shm->status.respiration.rate < 15)
	{
		nextBreathTime = now + ((breathInterval / 2) + 60);
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

	printf("Pulse is on port %d\n", portno);

	// Seed rand, needed for vpc array generation
	srand(NULL);

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
	start_task("pulseProcessChild", pulseProcessChild);
	start_task("pulseTimer", pulseTimer);
	start_task("pulseBroadcastLoop", pulseBroadcastLoop);
	
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

	if ( ::bind(sfd, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR )
	{
		//We couldn't bind (this will happen if you try to bind to the same  
		//socket more than once)
		cout << "pulseProcess - bind(): SOCKET_ERROR " << GetLastErrorAsString();
		return false;
	}

	listen(sfd, SOMAXCONN);
	socklen = sizeof(struct sockaddr_in);

	while (1)
	{
		cfd = accept(sfd, (struct sockaddr*)&client_addr, &socklen);
		if (cfd >= 0)
		{
			for (i = 0; i < MAX_LISTENERS; i++)
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
					break;
				}
			}
			if (i == MAX_LISTENERS)
			{
				// Unable to allocate
				closesocket(cfd);
			}
		}
	}
	sprintf_s(p_msg, BUF_SIZE, "simpulse terminates");
	log_message("", p_msg);
	exit(0);
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
		if (nextBreathTime <= now)
		{
			breath_beat_handler();
			nextBreathTime += breathInterval;
			if (nextBreathTime <= now)
			{
				nextBreathTime = now + breathInterval;
			}
		}
	}
}
void
pulseBroadcastLoop(void)
{
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
				printf("Pulse sent to %d listeners\n", count);
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
				printf("PulseVPC sent to %d listeners\n", count);
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
				printf("Breath sent to %d listeners\n", count);
			}
#endif
		}
	}
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
			sprintf(p_msg, "Set Pulse to %d", currentPulseRate);
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
			sprintf(p_msg, "Set Breath to %d", currentBreathRate);
			log_message("", p_msg);
#endif
		}
	}
}
