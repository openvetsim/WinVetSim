/*
 * VetSim.cpp
 *
 * SimMgr applicatiopn
 *
 * This file is part of the sim-mgr distribution.
 *
 * Copyright (c) 2019-2021 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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

#include "../vetsim.h"

// FOR WIN32 SIGNAL HANDLING
// compile with: /EHsc /W4
// Use signal to attach a signal handler to the abort routine
#include <stdlib.h>
#include <signal.h>

struct simmgr_shm shmSpace;
#define BUF_SIZE 2048
char msg_buf[2048];
int iiLockTaken = 0;

int lastEventLogged = 0;
int lastCommentLogged = 0;

void simmgrInitialize(void);
void resetAllParameters(void);
void clearAllTrends(void);
void hrcheck_handler(
	HWND hwnd,        // handle to window for timer messages 
	UINT message,     // WM_TIMER message 
	UINT idTimer,     // timer identifier 
	DWORD dwTime);
HWND GetConsoleHwnd(void);
ScenarioState scenario_state = ScenarioStopped;
NibpState nibp_state = NibpIdle;
#ifdef WIN32
time_t nibp_next_time;
time_t nibp_run_complete_time;
#else
std::time_t nibp_next_time;
std::time_t nibp_run_complete_time;
#endif

int main()
{
	printf("Starting\n");
	initSHM(1, 0);
	int last = -1;
	int count = 0;
	simmgrInitialize();

	printf("Hostname: %s\n", simmgr_shm->server.name);
	sprintf_s(msg_buf, BUF_SIZE, "%s", "Done");

	log_message("", msg_buf);
	GetConsoleHwnd();
	while (1)
	{
		Sleep(1000);
		if (last != simmgr_shm->status.cardiac.pulseCount)
		{
			last = simmgr_shm->status.cardiac.pulseCount;
			printf("Beat: %d\n", last);
		}
		if (count++ > 5)
		{
			break;
		}
	}

}

HWND GetConsoleHwnd(void)
{
#define MY_BUFSIZE 1024 // Buffer size for console window titles.
	HWND hwndFound;         // This is what is returned to the caller.
	char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
										// WindowTitle.
	char pszOldWindowTitle[MY_BUFSIZE]; // Contains original
										// WindowTitle.

	// Fetch current window title.

	GetConsoleTitle((LPWSTR)pszOldWindowTitle, MY_BUFSIZE);

	// Format a "unique" NewWindowTitle.
	printf("Old Title %s\n", pszOldWindowTitle);
	wsprintf((LPWSTR)pszNewWindowTitle, (LPWSTR)"%d/%d",
		GetTickCount(),
		GetCurrentProcessId());

	printf("New Title %s\n", pszNewWindowTitle);
	// Change current window title.

	SetConsoleTitle((LPWSTR)pszNewWindowTitle);

	// Ensure window title has been updated.

	Sleep(40);

	// Look for NewWindowTitle.

	hwndFound = FindWindow(NULL, (LPWSTR)pszNewWindowTitle);

	// Restore original window title.

	SetConsoleTitle((LPWSTR)pszOldWindowTitle);

	return(hwndFound);
}

void
simmgrInitialize(void)
{
	char* ptr;
#ifdef WIN32
	typedef void (*SignalHandlerPointer)(int);
#else
	struct sigaction new_action;
	sigset_t mask;
#endif
	// struct itimerspec its;

	// Zero out the shared memory and reinit the values
	memset(simmgr_shm, 0, sizeof(struct simmgr_shm));

	// hdr
	simmgr_shm->hdr.version = SIMMGR_VERSION;
	simmgr_shm->hdr.size = sizeof(struct simmgr_shm);


	// server
	do_command_read("hostname.exe", simmgr_shm->server.name, sizeof(simmgr_shm->server.name) - 1);
	ptr = getETH0_IP();
	sprintf_s(simmgr_shm->server.ip_addr, STR_SIZE, "%s", ptr);
	// server_time and msec_time are updated in the loop

	resetAllParameters();

	// status/scenario
	sprintf_s(simmgr_shm->status.scenario.active, STR_SIZE, "%s", "default");
	sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "%s", "Stopped");
	simmgr_shm->status.scenario.record = 0;

	// instructor/sema
#ifdef WIN32
	simmgr_shm->instructor.sema = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
#else
	sem_init(&simmgr_shm->instructor.sema, 1, 1); // pshared =1, value =1
#endif
	iiLockTaken = 0;

	// instructor/scenario
	sprintf_s(simmgr_shm->instructor.scenario.active, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "");
	simmgr_shm->instructor.scenario.record = -1;

	// Log File
#ifdef WIN32
	simmgr_shm->logfile.sema = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
#else
	sem_init(&simmgr_shm->logfile.sema, 1, 1); // pshared =1, value =1
#endif
	simmgr_shm->logfile.active = 0;
	sprintf_s(simmgr_shm->logfile.filename, FILENAME_SIZE, "%s", "");
	simmgr_shm->logfile.lines_written = 0;

	// Event List
	simmgr_shm->eventListNext = 0;
	lastEventLogged = 0;

	// Comment List
	simmgr_shm->commentListNext = 0;
	lastCommentLogged = 0;

	// instructor/cpr
	simmgr_shm->instructor.cpr.compression = -1;
	simmgr_shm->instructor.cpr.last = -1;
	simmgr_shm->instructor.cpr.release = -1;
	simmgr_shm->instructor.cpr.duration = -1;
	// instructor/defibrillation
	simmgr_shm->instructor.defibrillation.last = -1;
	simmgr_shm->instructor.defibrillation.energy = -1;
	simmgr_shm->instructor.defibrillation.shock = -1;

	clearAllTrends();

#ifdef WIN32
#define IDT_HR_TIMER	1

	SetTimer(
		NULL, // hWnd
		IDT_HR_TIMER, // nIDEvent
		25, // time in msec
		(TIMERPROC)hrcheck_handler // Process function
	);
#else
	// Use a timer for HR checks
	new_action.sa_flags = SA_SIGINFO;
	new_action.sa_sigaction = hrcheck_handler;
	sigemptyset(&new_action.sa_mask);

#define HRCHECK_TIMER_SIG	(SIGRTMIN+2)
	if (sigaction(HRCHECK_TIMER_SIG, &new_action, NULL) == -1)
	{
		perror("sigaction");
		sprintf_s(msgbuf, BUF_SIZE, "sigaction() fails for HRCheck Timer: %s", strerror(errno));
		log_message("", msgbuf);
		exit(-1);
	}
	// Block timer signal temporarily
	sigemptyset(&mask);
	sigaddset(&mask, HRCHECK_TIMER_SIG);
	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
	{
		perror("sigprocmask");
		sprintf_s(msgbuf, BUF_SIZE, "sigprocmask() fails for Pulse Timer %s", strerror(errno));
		log_message("", msgbuf);
		exit(-1);
	}
	// Create the Timer
	hrcheck_sev.sigev_notify = SIGEV_SIGNAL;
	hrcheck_sev.sigev_signo = HRCHECK_TIMER_SIG;
	hrcheck_sev.sigev_value.sival_ptr = &hrcheck_timer;

	if (timer_create(CLOCK_REALTIME, &hrcheck_sev, &hrcheck_timer) == -1)
	{
		perror("timer_create");
		sprintf_s(msgbuf, BUF_SIZE, "timer_create() fails for HRCheck Timer %s", strerror(errno));
		log_message("", msgbuf);
		exit(-1);
	}
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
	{
		perror("sigprocmask");
		sprintf_s(msgbuf, BUF_SIZE, "sigprocmask() fails for HRCheck Timer%s ", strerror(errno));
		log_message("", msgbuf);
		exit(-1);
	}
	its.it_interval.tv_sec = (long int)0;
	its.it_interval.tv_nsec = (long int)(5 * 1000 * 1000);	// 10 msec
	its.it_value.tv_sec = (long int)0;
	its.it_value.tv_nsec = (long int)(5 * 1000 * 1000);	// 10 msec
	timer_settime(hrcheck_timer, 0, &its, NULL);
#endif
}


void
resetAllParameters(void)
{
	// status/cardiac
	sprintf_s(simmgr_shm->status.cardiac.rhythm, STR_SIZE, "%s", "sinus");
	sprintf_s(simmgr_shm->status.cardiac.vpc, STR_SIZE, "%s", "none");
	sprintf_s(simmgr_shm->status.cardiac.vfib_amplitude, STR_SIZE, "%s", "high");
	simmgr_shm->status.cardiac.vpc_freq = 10;
	simmgr_shm->status.cardiac.vpc_delay = 0;
	simmgr_shm->status.cardiac.pea = 0;
	simmgr_shm->status.cardiac.rate = 80;
	simmgr_shm->status.cardiac.nibp_rate = 80;
	simmgr_shm->status.cardiac.nibp_read = -1;
	simmgr_shm->status.cardiac.nibp_linked_hr = 1;
	simmgr_shm->status.cardiac.nibp_freq = 0;
	sprintf_s(simmgr_shm->status.cardiac.pwave, STR_SIZE, "%s", "none");
	simmgr_shm->status.cardiac.pr_interval = 140; // Good definition at http://lifeinthefastlane.com/ecg-library/basics/pr-interval/
	simmgr_shm->status.cardiac.qrs_interval = 85;
	simmgr_shm->status.cardiac.bps_sys = 105;
	simmgr_shm->status.cardiac.bps_dia = 70;
	simmgr_shm->status.cardiac.right_dorsal_pulse_strength = 2;
	simmgr_shm->status.cardiac.right_femoral_pulse_strength = 2;
	simmgr_shm->status.cardiac.left_dorsal_pulse_strength = 2;
	simmgr_shm->status.cardiac.left_femoral_pulse_strength = 2;
	sprintf_s(simmgr_shm->status.cardiac.heart_sound, STR_SIZE, "%s", "none");
	simmgr_shm->status.cardiac.heart_sound_volume = 10;
	simmgr_shm->status.cardiac.heart_sound_mute = 0;
	simmgr_shm->status.cardiac.ecg_indicator = 0;
	simmgr_shm->status.cardiac.bp_cuff = 0;
	simmgr_shm->status.cardiac.arrest = 0;

	// status/respiration
	sprintf_s(simmgr_shm->status.respiration.left_lung_sound, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.left_sound_in, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.left_sound_out, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.left_sound_back, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.right_lung_sound, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.right_sound_in, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.right_sound_out, STR_SIZE, "%s", "normal");
	sprintf_s(simmgr_shm->status.respiration.right_sound_back, STR_SIZE, "%s", "normal");
	simmgr_shm->status.respiration.left_lung_sound_volume = 10;
	simmgr_shm->status.respiration.left_lung_sound_mute = 1;
	simmgr_shm->status.respiration.right_lung_sound_volume = 10;
	simmgr_shm->status.respiration.right_lung_sound_mute = 0;
	simmgr_shm->status.respiration.inhalation_duration = 1350;
	simmgr_shm->status.respiration.exhalation_duration = 1050;
	simmgr_shm->status.respiration.rate = 20;
	simmgr_shm->status.respiration.spo2 = 95;
	simmgr_shm->status.respiration.etco2 = 34;
	simmgr_shm->status.respiration.etco2_indicator = 0;
	simmgr_shm->status.respiration.spo2_indicator = 0;
	simmgr_shm->status.respiration.chest_movement = 0;
	simmgr_shm->status.respiration.manual_breath = 0;
	simmgr_shm->status.respiration.manual_count = 0;
	awrr_restart();

	// status/vocals
	sprintf_s(simmgr_shm->status.vocals.filename, STR_SIZE, "%s", "");
	simmgr_shm->status.vocals.repeat = 0;
	simmgr_shm->status.vocals.volume = 10;
	simmgr_shm->status.vocals.play = 0;
	simmgr_shm->status.vocals.mute = 0;

	// status/auscultation
	simmgr_shm->status.auscultation.side = 0;
	simmgr_shm->status.auscultation.row = 0;
	simmgr_shm->status.auscultation.col = 0;

	// status/pulse
	simmgr_shm->status.pulse.right_dorsal = 0;
	simmgr_shm->status.pulse.left_dorsal = 0;
	simmgr_shm->status.pulse.right_femoral = 0;
	simmgr_shm->status.pulse.left_femoral = 0;

	// status/cpr
	simmgr_shm->status.cpr.last = 0;
	simmgr_shm->status.cpr.compression = 0;
	simmgr_shm->status.cpr.release = 0;
	simmgr_shm->status.cpr.duration = 0;

	// status/defibrillation
	simmgr_shm->status.defibrillation.last = 0;
	simmgr_shm->status.defibrillation.energy = 100;
	simmgr_shm->status.defibrillation.shock = 0;

	// status/general
	simmgr_shm->status.general.temperature = 1017;
	simmgr_shm->status.general.temperature_enable = 0;
	//sprintf_s(simmgr_shm->status.general.temperature_units, STR_SIZE, "%s", "F" ); 

	// status/media
	sprintf_s(simmgr_shm->status.media.filename, FILENAME_SIZE, "%s", "");
	simmgr_shm->status.media.play = 0;

	// status/telesim
	// simmgr_shm->status.telesim.enable = 0;
	sprintf_s(simmgr_shm->status.telesim.vid[0].name, STR_SIZE, "%s", "");
	simmgr_shm->status.telesim.vid[0].command = 0;
	simmgr_shm->status.telesim.vid[0].param = 0;
	simmgr_shm->status.telesim.vid[0].next = 0;
	sprintf_s(simmgr_shm->status.telesim.vid[1].name, STR_SIZE, "%s", "");
	simmgr_shm->status.telesim.vid[1].command = 0;
	simmgr_shm->status.telesim.vid[1].param = 0;
	simmgr_shm->status.telesim.vid[1].next = 0;

	// instructor/cardiac
	sprintf_s(simmgr_shm->instructor.cardiac.rhythm, STR_SIZE, "%s", "");
	simmgr_shm->instructor.cardiac.rate = -1;
	simmgr_shm->instructor.cardiac.nibp_rate = -1;
	simmgr_shm->instructor.cardiac.nibp_read = -1;
	simmgr_shm->instructor.cardiac.nibp_linked_hr = -1;
	simmgr_shm->instructor.cardiac.nibp_freq = -1;
	sprintf_s(simmgr_shm->instructor.cardiac.pwave, STR_SIZE, "%s", "");
	simmgr_shm->instructor.cardiac.pr_interval = -1;
	simmgr_shm->instructor.cardiac.qrs_interval = -1;
	simmgr_shm->instructor.cardiac.bps_sys = -1;
	simmgr_shm->instructor.cardiac.bps_dia = -1;
	simmgr_shm->instructor.cardiac.pea = -1;
	simmgr_shm->instructor.cardiac.vpc_freq = -1;
	simmgr_shm->instructor.cardiac.vpc_delay = -1;
	sprintf_s(simmgr_shm->instructor.cardiac.vpc, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.cardiac.vfib_amplitude, STR_SIZE, "%s", "");
	simmgr_shm->instructor.cardiac.right_dorsal_pulse_strength = -1;
	simmgr_shm->instructor.cardiac.right_femoral_pulse_strength = -1;
	simmgr_shm->instructor.cardiac.left_dorsal_pulse_strength = -1;
	simmgr_shm->instructor.cardiac.left_femoral_pulse_strength = -1;
	sprintf_s(simmgr_shm->instructor.cardiac.heart_sound, STR_SIZE, "%s", "");
	simmgr_shm->instructor.cardiac.heart_sound_volume = -1;
	simmgr_shm->instructor.cardiac.heart_sound_mute = -1;
	simmgr_shm->instructor.cardiac.ecg_indicator = -1;
	simmgr_shm->instructor.cardiac.bp_cuff = -1;
	simmgr_shm->instructor.cardiac.arrest = -1;

	// instructor/respiration
	sprintf_s(simmgr_shm->instructor.respiration.left_lung_sound, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.left_sound_in, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.left_sound_out, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.left_sound_back, STR_SIZE, "%s", "");
	simmgr_shm->instructor.respiration.left_lung_sound_volume = -1;
	simmgr_shm->instructor.respiration.left_lung_sound_mute = -1;
	simmgr_shm->instructor.respiration.right_lung_sound_volume = -1;
	simmgr_shm->instructor.respiration.right_lung_sound_mute = -1;
	sprintf_s(simmgr_shm->instructor.respiration.right_lung_sound, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.right_sound_in, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.right_sound_out, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.respiration.right_sound_back, STR_SIZE, "%s", "");
	simmgr_shm->instructor.respiration.inhalation_duration = -1;
	simmgr_shm->instructor.respiration.exhalation_duration = -1;
	simmgr_shm->instructor.respiration.rate = -1;
	simmgr_shm->instructor.respiration.spo2 = -1;
	simmgr_shm->instructor.respiration.etco2 = -1;
	simmgr_shm->instructor.respiration.etco2_indicator = -1;
	simmgr_shm->instructor.respiration.spo2_indicator = -1;
	simmgr_shm->instructor.respiration.chest_movement = -1;
	simmgr_shm->instructor.respiration.manual_breath = -1;
	simmgr_shm->instructor.respiration.manual_count = -1;

	// instructor/media
	sprintf_s(simmgr_shm->instructor.media.filename, FILENAME_SIZE, "%s", "");
	simmgr_shm->instructor.media.play = -1;

	// instructor/telesim
	simmgr_shm->instructor.telesim.enable = -1;
	sprintf_s(simmgr_shm->instructor.telesim.vid[0].name, STR_SIZE, "%s", "");
	simmgr_shm->instructor.telesim.vid[0].command = -1;
	simmgr_shm->instructor.telesim.vid[0].param = -1;
	simmgr_shm->instructor.telesim.vid[0].next = -1;
	sprintf_s(simmgr_shm->instructor.telesim.vid[1].name, STR_SIZE, "%s", "");
	simmgr_shm->instructor.telesim.vid[1].command = -1;
	simmgr_shm->instructor.telesim.vid[1].param = -1;
	simmgr_shm->instructor.telesim.vid[1].next = -1;

	// instructor/general
	simmgr_shm->instructor.general.temperature = -1;
	simmgr_shm->instructor.general.temperature_enable = -1;
	sprintf_s(simmgr_shm->instructor.general.temperature_units, STR_SIZE, "%s", "");

	// instructor/vocals
	sprintf_s(simmgr_shm->instructor.vocals.filename, FILENAME_SIZE, "%s", "");
	simmgr_shm->instructor.vocals.repeat = -1;
	simmgr_shm->instructor.vocals.volume = -1;
	simmgr_shm->instructor.vocals.play = -1;
	simmgr_shm->instructor.vocals.mute = -1;

	// instructor/defibrillation
	simmgr_shm->instructor.defibrillation.energy = -1;
	simmgr_shm->instructor.defibrillation.shock = -1;

	clearAllTrends();
}

/*
 * awrr_check
 *
 * Calculate awrr based on count of breaths, both manual and 'normal'
 *
 * 1 - Detect and log breath when either natural or manual start
 * 2 - If no breaths are recorded in the past 20 seconds (excluding the past 2 seconds) report AWRR as zero
 * 3 - Calculate AWRR based on the average time of the recorded breaths within the past 90 seconds, excluding the past 2 seconds
 *
*/
#define BREATH_CALC_LIMIT		4		// Max number of recorded breaths to count in calculation
#define BREATH_LOG_LEN	128
unsigned int breathLog[BREATH_LOG_LEN] = { 0, };
unsigned int breathLogNext = 0;

#define BREATH_LOG_STATE_IDLE	0
#define BREATH_LOG_STATE_DETECT	1
int breathLogState = BREATH_LOG_STATE_IDLE;

unsigned int breathLogLastNatural = 0;	// breathCount, last natural
unsigned int breathLogLastManual = 0;	// manual_count, last manual
unsigned int breathLogLast = 0;			// Time of last breath

#define BREATH_LOG_DELAY	(2)
int breathLogDelay = 0;

#define BREATH_LOG_CHANGE_LOOPS	1
int breathLogReportLoops = 0;

void
awrr_restart(void)
{
	breathLogLastNatural = 0;
	breathLogLastManual = 0;
	breathLogNext = 0;
	breathLogLast = 0;
	breathLogDelay = 0;
	breathLogReportLoops = 0;
	int now = simmgr_shm->server.msec_time; //  time(NULL);	// Current sec time
	breathLog[breathLogNext] = now - 40000;
	breathLogNext += 1;
	breathLog[breathLogNext] = now - 39000;
	breathLogNext += 1;
	breathLog[breathLogNext] = now - 38000;
	breathLogNext += 1;
}
void
awrr_check(void)
{
	int now = simmgr_shm->server.msec_time; //  time(NULL);	// Current sec time
	int prev;
	int breaths;
	int totalTime;
	unsigned int lastTime;
	unsigned int firstTime;
	int diff;
	float awRR;
	int i;
	int intervals;
	int oldRate;
	int newRate;

	oldRate = simmgr_shm->status.respiration.awRR;

	// Breath Detect and Log
	if (breathLogState == BREATH_LOG_STATE_DETECT)
	{
		// After detect, wait 400 msec (two calls) before another detect allowed
		if (breathLogDelay++ >= BREATH_LOG_DELAY)
		{
			breathLogState = BREATH_LOG_STATE_IDLE;
			breathLogLastNatural = simmgr_shm->status.respiration.breathCount;
			breathLogLastManual = simmgr_shm->status.respiration.manual_count;
		}
	}
	else if (breathLogState == BREATH_LOG_STATE_IDLE)
	{
		if (breathLogLastNatural != simmgr_shm->status.respiration.breathCount)
		{
			breathLogState = BREATH_LOG_STATE_DETECT;
			breathLogLastNatural = simmgr_shm->status.respiration.breathCount;
			breathLogLast = now;
			breathLog[breathLogNext] = now;
			breathLogDelay = 0;
			breathLogNext += 1;
			if (breathLogNext >= BREATH_LOG_LEN)
			{
				breathLogNext = 0;
			}
		}
		else if (breathLogLastManual != simmgr_shm->status.respiration.manual_count)
		{
			breathLogState = BREATH_LOG_STATE_DETECT;
			breathLogLastManual = simmgr_shm->status.respiration.manual_count;
			breathLogLast = now;
			breathLog[breathLogNext] = now;
			breathLogDelay = 0;
			breathLogNext += 1;
			if (breathLogNext >= BREATH_LOG_LEN)
			{
				breathLogNext = 0;
			}
		}
	}

	// AWRR Calculation - Look at no more than BREATH_CALC_LIMIT breaths - Skip if no breaths within 20 seconds
	lastTime = 0;
	firstTime = 0;
	prev = breathLogNext - 1;
	if (prev < 0)
	{
		prev = BREATH_LOG_LEN - 1;
	}
	breaths = 0;
	intervals = 0;

	lastTime = breathLog[prev];
	if (lastTime <= 0)  // Don't look at empty logs
	{
		simmgr_shm->status.respiration.awRR = 0;
	}
	else
	{
		diff = now - lastTime;
		if (diff > 20000)
		{
			simmgr_shm->status.respiration.awRR = 0;
		}
		else
		{
			prev -= 1;
			if (prev < 0)
			{
				prev = BREATH_LOG_LEN - 1;
			}
			for (i = 0; i < BREATH_CALC_LIMIT; i++)
			{
				diff = now - breathLog[prev];
				if (diff > 47000) // Over Limit seconds since this recorded breath
				{
					break;
				}
				else
				{
					firstTime = breathLog[prev]; // Recorded start of the first breath
					breaths += 1;
					intervals += 1;
					prev -= 1;
					if (prev < 0)
					{
						prev = BREATH_LOG_LEN - 1;
					}
				}
			}
		}
		if (intervals > 2)
		{
			totalTime = lastTime - firstTime;
			if (totalTime == 0)
			{
				awRR = 0;
			}
			else
			{
				awRR = ((((float)intervals / (float)totalTime)) * 60000);
			}
			if (awRR < 0)
			{
				awRR = 0;
			}
			else if (awRR > 60)
			{
				awRR = 60;
			}
		}
		else
		{
			awRR = 0;
		}
		if (breathLogReportLoops++ == BREATH_LOG_CHANGE_LOOPS)
		{
			breathLogReportLoops = 0;
			newRate = (int)roundf(awRR);
			if (oldRate != newRate)
			{
				// setRespirationPeriods(oldRate, newRate );
				simmgr_shm->status.respiration.awRR = newRate;
			}
		}
	}
}

long int cprLast = 0;
long int cprRunTime = 0;
long int cprDuration = 2000;

void
cpr_check(void)
{
	long int now = simmgr_shm->server.msec_time;
	long int cprCurrent = simmgr_shm->status.cpr.last;

	if (cprCurrent != cprLast)
	{
		if (cprCurrent == 0)
		{
			simmgr_shm->status.cpr.running = 0;
			cprLast = cprCurrent;
		}
		else
		{
			cprRunTime = now;
			simmgr_shm->status.cpr.running = 1;
			cprLast = cprCurrent;
		}
	}
	if (simmgr_shm->status.cpr.running > 0)
	{
		if (simmgr_shm->status.cpr.compression > 0)
		{
			cprRunTime = now;
		}
		else if ((cprRunTime + cprDuration) < now)
		{
			simmgr_shm->status.cpr.running = 0;
		}
	}
}
long int shockLast = 0;
long int shockStartTime = 0;
long int shockDuration = 2000;

void
shock_check(void)
{
	long int now = simmgr_shm->server.msec_time;
	long int shockCurrent = simmgr_shm->status.defibrillation.last;

	if (shockCurrent != shockLast)
	{
		if (shockCurrent == 0)
		{
			simmgr_shm->status.defibrillation.shock = 0;
			shockLast = shockCurrent;
		}
		else
		{
			shockStartTime = now;
			simmgr_shm->status.defibrillation.shock = 1;
			shockLast = shockCurrent;
		}
	}
	if (simmgr_shm->status.defibrillation.shock > 0)
	{
		if ((shockStartTime + shockDuration) < now)
		{
			simmgr_shm->status.defibrillation.shock = 0;
		}
	}
}
/*
 * hrcheck_handler
 *
 * Calculate heart rate based on count of beats, (normal, CPR and  VPC)
 *
 * 1 - Detect and log beat when any of natural, vpc or CPR
 * 2 - If no beats are recorded in the past 20 seconds (excluding the past 2 seconds) report rate as zero
 * 3 - Calculate beats based on the average time of the recorded beats within the past 90 seconds, excluding the past 2 seconds
 *
*/
#define HR_CALC_LIMIT		10		// Max number of recorded beats to count in calculation
#define HR_LOG_LEN	128
unsigned long int hrLog[HR_LOG_LEN] = { 0, };
int hrLogNext = 0;

unsigned int hrLogLastNatural = 0;	// beatCount, last natural
unsigned int hrLogLastVPC = 0;	// VPC count, last VPC

#define HR_LOG_DELAY	(40)
int hrLogDelay = 0;

#define HR_LOG_CHANGE_LOOPS	50	// hr_check is called every 5 msec. So this will cause a recalc every 250 msec
int hrLogReportLoops = 0;

#ifdef WIN32
void hrcheck_handler(
	HWND hwnd,        // handle to window for timer messages 
	UINT message,     // WM_TIMER message 
	UINT idTimer,     // timer identifier 
	DWORD dwTime)     // current system time  )    // additional information 
#else
static void
hrcheck_handler(int sig, siginfo_t* si, void* uc)
#endif
{
	long int now; // Current msec time
	long int prev;
	int beats;
	long int totalTime;
	long int lastTime;
	long int firstTime;
	long int diff;
	float avg_rate;
	float seconds;
	float minutes;
	int i;
	int intervals;
	/*
	int oldRate;
	int newRate;
	*/
	int newBeat = 0;

	now = msec_time_update();

	if (simmgr_shm->status.cpr.running)
	{
		hrLogLastNatural = simmgr_shm->status.cardiac.pulseCount;
		hrLogLastVPC = simmgr_shm->status.cardiac.pulseCountVpc;
		simmgr_shm->status.cardiac.avg_rate = 0;
		firstTime = now - 30000;
		for (i = 0; i < HR_LOG_LEN; i++)
		{
			hrLog[i] = firstTime;
		}
		return;
	}
	else if (hrLogLastNatural != simmgr_shm->status.cardiac.pulseCount)
	{
		hrLogLastNatural = simmgr_shm->status.cardiac.pulseCount;
		newBeat = 1;
	}
	else if (hrLogLastVPC != simmgr_shm->status.cardiac.pulseCountVpc)
	{
		hrLogLastVPC = simmgr_shm->status.cardiac.pulseCountVpc;
		newBeat = 1;
	}

	if (newBeat)
	{
		prev = hrLogNext;
		hrLog[hrLogNext] = now;
		hrLogNext += 1;
		if (hrLogNext >= HR_LOG_LEN)
		{
			hrLogNext = 0;
		}
	}
	else
	{
		prev = hrLogNext - 1;
		if (prev < 0)
		{
			prev = (HR_LOG_LEN - 1);
		}
	}
	if (hrLogReportLoops++ >= HR_LOG_CHANGE_LOOPS)
	{
		hrLogReportLoops = 0;
		// AVG Calculation - Look at no more than 10 beats - Skip if no beats within 20 seconds
		lastTime = 0;
		firstTime = 0;

		beats = 1;
		intervals = 0;

		lastTime = hrLog[prev];
		if (lastTime < 0)  // Don't look at empty logs
		{
			simmgr_shm->status.cardiac.avg_rate = 2;
		}
		else if (lastTime == 0)  // Don't look at empty logs
		{
			simmgr_shm->status.cardiac.avg_rate = 3;
		}
		else
		{
			diff = now - lastTime;
			if (diff > 20000)
			{
				simmgr_shm->status.cardiac.avg_rate = 4;
			}
			else
			{
				prev -= 1;
				if (prev < 0)
				{
					prev = (HR_LOG_LEN - 1);
				}
				for (i = 0; i < HR_CALC_LIMIT; i++)
				{
					diff = now - hrLog[prev];
					if (diff > 20000) // Over Limit seconds since this recorded beat
					{
						break;
					}
					else
					{
						firstTime = hrLog[prev]; // Recorded start of the first beat
						beats += 1;
						intervals += 1;
						prev -= 1;
						if (prev < 0)
						{
							prev = HR_LOG_LEN - 1;
						}
					}
				}
			}

			if (intervals > 0)
			{
				totalTime = lastTime - firstTime;
				if (totalTime == 0)
				{
					avg_rate = 7;
				}
				else
				{
					seconds = (float)totalTime / 1000;
					minutes = seconds / 60;
					avg_rate = (float)intervals / minutes;
				}
				if (avg_rate < 0)
				{
					avg_rate = 6;
				}
				else if (avg_rate > 360)
				{
					avg_rate = 360;
				}
			}
			else
			{
				avg_rate = 5;
			}
			simmgr_shm->status.cardiac.avg_rate = (int)round(avg_rate);
		}
	}
}
/*
 * msec_timer_update
*/
long int
msec_time_update(void)
{
	long int msec;
#ifdef WIN32
	msec = GetTickCount();
#else
	struct timeval tv;
	int sts;

	sts = gettimeofday(&tv, NULL);
	if (sts)
	{
		msec = 0;
	}
	else
	{
		msec = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}
#endif
	simmgr_shm->server.msec_time = msec;
	return (msec);
}
/*
 * time_update
 *
 * Get the localtime and write it as a string to the SHM data
 */
int last_time_sec = -1;

#ifdef WIN32
time_t scenario_start_time;
time_t scenario_run_time;
#else
// Time values, to track start time and elapsed time
// This is the "absolute" time
std::time_t scenario_start_time;
std::time_t scenario_run_time;
#endif

void
time_update(void)
{
	int hour;
	int min;
	int elapsedTimeSeconds;
	int seconds;
	int sec;
	double temperature;
	char buf[1024];

#ifdef WIN32
	time_t now;
	SYSTEMTIME st;

	GetLocalTime(&st);
	sprintf_s(simmgr_shm->server.server_time, STR_SIZE, "%04d/%02d/%02d %02d:%02d:%02d",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond);
	now = time(nullptr);
	elapsedTimeSeconds = (int)difftime(now, scenario_start_time);
#else
	struct tm tm;
	time_t the_time;
	std::time_t now;
	the_time = time(NULL);

	(void)localtime_r(&the_time, &tm);
	(void)asctime_r(&tm, buf);
	strtok(buf, "\n");		// Remove Line Feed
	sprintf_s(simmgr_shm->server.server_time, STR_SIZE, "%s", buf);
	now = std::time(nullptr);
	elapsedTimeSeconds = (int)difftime(now, scenario_start_time);
#endif

	if ((scenario_state == ScenarioRunning) ||
		(scenario_state == ScenarioPaused))
	{
		sec = elapsedTimeSeconds;
		min = (sec / 60);
		hour = min / 60;
		sprintf_s(simmgr_shm->status.scenario.runtimeAbsolute, STR_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);

		sec = elapsedTimeSeconds + simmgr_shm->status.general.clockStartSec;
		min = (sec / 60);
		hour = min / 60;
		sprintf_s(simmgr_shm->status.scenario.clockDisplay, STR_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);
	}
	if ((elapsedTimeSeconds > MAX_SCENARIO_RUNTIME) &&
		((scenario_state == ScenarioRunning) ||
			(scenario_state == ScenarioPaused)))
	{
		sprintf_s(buf, BUF_SIZE, "Scenario: MAX Scenario Runtime exceeded. Terminating.");
		simlog_entry(buf);

		takeInstructorLock();
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "Terminate");
		releaseInstructorLock();
	}
	else if (scenario_state == ScenarioRunning)
	{
		sec = simmgr_shm->status.scenario.elapsed_msec_scenario / 1000;
		min = (sec / 60);
		hour = min / 60;
		sprintf_s(simmgr_shm->status.scenario.runtimeScenario, STR_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);

		sec = simmgr_shm->status.scenario.elapsed_msec_scene / 1000;
		min = (sec / 60);
		hour = min / 60;
		sprintf_s(simmgr_shm->status.scenario.runtimeScene, STR_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);

		seconds = elapsedTimeSeconds % 60;
		if ((seconds == 0) && (last_time_sec != 0))
		{
			temperature = (float)simmgr_shm->status.general.temperature / 10;
			if (simmgr_shm->status.general.temperature_units[0] == 'C')
			{
				temperature = (temperature - 32) * 0.556;
			}
			// Do periodic Stats update every minute
			sprintf_s(buf, BUF_SIZE, "VS: Temp: %0.1f %s; RR: %d; awRR: %d; HR: %d; %s; BP: %d/%d; SPO2: %d; etCO2: %d mmHg; Probes: ECG: %s; BP: %s; SPO2: %s; ETCO2: %s; Temp %s",
				temperature, simmgr_shm->status.general.temperature_units,
				simmgr_shm->status.respiration.rate,
				simmgr_shm->status.respiration.awRR,
				simmgr_shm->status.cardiac.rate,
				(simmgr_shm->status.cardiac.arrest == 1 ? "Arrest" : "Normal"),
				simmgr_shm->status.cardiac.bps_sys,
				simmgr_shm->status.cardiac.bps_dia,
				simmgr_shm->status.respiration.spo2,
				simmgr_shm->status.respiration.etco2,
				(simmgr_shm->status.cardiac.ecg_indicator == 1 ? "on" : "off"),
				(simmgr_shm->status.cardiac.bp_cuff == 1 ? "on" : "off"),
				(simmgr_shm->status.respiration.spo2_indicator == 1 ? "on" : "off"),
				(simmgr_shm->status.respiration.etco2_indicator == 1 ? "on" : "off"),
				(simmgr_shm->status.general.temperature_enable == 1 ? "on" : "off")
			);
			simlog_entry(buf);
		}
		last_time_sec = seconds;
	}
	else if (scenario_state == ScenarioStopped)
	{
		last_time_sec = -1;
	}
}
/*
 * comm_check
 *
 * verify that the communications path to the SimCtl is open and ok.
 * If not, try to reestablish.
 */
void
comm_check(void)
{
	// TBD
}

/*
 * Cardiac Process
 *
 * Based on the rate and target selected, modify the pulse rate
 */
struct trend cardiacTrend;
struct trend respirationTrend;
struct trend sysTrend;
struct trend diaTrend;
struct trend tempTrend;
struct trend spo2Trend;
struct trend etco2Trend;

int
clearTrend(struct trend* trend, int current)
{
	trend->end = current;
	trend->current = current;

	return ((int)trend->current);
}

void
clearAllTrends(void)
{
	// Clear running trends
	(void)clearTrend(&cardiacTrend, simmgr_shm->status.cardiac.rate);
	(void)clearTrend(&sysTrend, simmgr_shm->status.cardiac.bps_sys);
	(void)clearTrend(&diaTrend, simmgr_shm->status.cardiac.bps_dia);
	(void)clearTrend(&respirationTrend, simmgr_shm->status.respiration.rate);
	(void)clearTrend(&spo2Trend, simmgr_shm->status.respiration.spo2);
	(void)clearTrend(&etco2Trend, simmgr_shm->status.respiration.etco2);
	(void)clearTrend(&tempTrend, simmgr_shm->status.general.temperature);
}