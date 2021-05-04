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

#include "vetsim.h"

// FOR WIN32 SIGNAL HANDLING
// compile with: /EHsc /W4
// Use signal to attach a signal handler to the abort routine

#include <signal.h>

int lastEventLogged = 0;
int lastCommentLogged = 0;
char buf[1024];

int scenarioPid = -1;
char sesid[1024] = { 0, };
char c_msgbuf[MSG_LENGTH];
char sessionsPath[1088];

// Time values, to track start time and elapsed time
// This is the "absolute" time
std::time_t scenario_start_time;
std::time_t now;
std::time_t scenario_run_time;

std::time_t nibp_next_time;
std::time_t nibp_run_complete_time;


#define SCENARIO_LOOP_COUNT		19	// Run the scenario every SCENARIO_LOOP_COUNT iterations of the 10 msec loop
#define SCENARIO_PROCCHECK  	1
#define SCENARIO_COMMCHECK  	5
#define SCENARIO_EVENTCHECK		10
#define SCENARIO_CPRCHECK		13
#define SCENARIO_SHOCKCHECK		14
#define SCENARIO_AWRRCHECK		15
#define SCENARIO_TIMECHECK		18

int scenarioCount = 0;
struct simmgr_shm shmSpace;
struct localConfiguration localConfig;
#define BUF_SIZE 2048
char msg_buf[BUF_SIZE];
int vs_iiLockTaken = 0;
int hrCheckCount = 0;

void simmgrInitialize(void);
void resetAllParameters(void);
void clearAllTrends(void);
void hrcheck_handler(void);
int updateScenarioState(ScenarioState new_state);

ScenarioState scenario_state = ScenarioState::ScenarioStopped;
NibpState nibp_state = NibpState::NibpIdle;


// Start a task to run every "interval" msec
void timer_start(std::function<void(void)> func, unsigned int interval)
{
	std::thread([func, interval]() {
		while (true)
		{
			func();
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		}
	}).detach();
}

// Start a task to run once. Might run forever.
void start_task(const char *name, std::function<void(void)> func)
{
	std::thread::id id;

	std::thread proc = std::thread([func]() {	func(); });
	proc.detach();
	id = proc.get_id();
	cout << "Task Started: " << name << " " << id << endl;
}

void SignalHandler(int signal)
{
	switch (signal)
	{
	case SIGABRT:
		printf("sig: Abort\n");
		break;
	case SIGFPE:
		printf("sig: Floating Point Error\n");
		break;
	case SIGILL:
		printf("sig: Illegal Instruction\n");
		break;
	case SIGINT:
		printf("sig: CTRL+C\n");
		break;
	case SIGSEGV:
		printf("sig: Segmentation Fault\n");
		break;
	case SIGTERM:
		printf("sig: Termination Request\n");
		break;
	}
	stopPHPServer();
}

void getObsHandle(int first, const char *appName);
int testKeys(void);
int getKeys(void);

int main()
{
	int last = -1;
	int count = 0;
	char cc;
	extern struct obsData obsd;
	setvbuf(stdout, NULL, _IONBF, 0);
	char cmd[BUF_SIZE];

	printf("Starting\n");

	// Set configurable parameters to defaults
	localConfig.port_pulse = DEFAULT_PORT_PULSE;
	localConfig.port_status = DEFAULT_PORT_STATUS;
	localConfig.php_server_port = DEFAULT_PHP_SERVER_PORT;
	sprintf_s(localConfig.php_server_addr, "%s", DEFAULT_PHP_SERVER_ADDRESS );
	sprintf_s(localConfig.log_name, "%s", DEFAULT_LOG_NAME );
	sprintf_s(localConfig.html_path, "%s", DEFAULT_HTML_PATH );

	// Allow parameters to be overridedn from registry
	getKeys();

	initSHM(1, 0);

	simmgrInitialize();
	start_task("pluseTask", pulseTask);
	start_task("simstatusMain", simstatusMain);

	

	printf("Hostname: %s\n", simmgr_shm->server.name);
	sprintf_s(msg_buf, BUF_SIZE, "%s", "Done");
	log_message("", msg_buf);

	//testKeys();
	//return(0);

	if (startPHPServer())
	{
		printf("Could not start PHP Server\n" );
		sprintf_s(msg_buf, BUF_SIZE, "%s", "Could not start PHP Server");
		log_message("", msg_buf);
		exit(-1);
	}
	// Open web page
	sprintf_s(cmd, BUF_SIZE, "start http://%s:%d/sim-ii", PHP_SERVER_ADDR, PHP_SERVER_PORT );
	system(cmd);
	
	obsd.obsWnd = NULL;

	while (1)
	{
		Sleep(10);
		if (last != simmgr_shm->status.cardiac.pulseCount)
		{
			last = simmgr_shm->status.cardiac.pulseCount;
		}
		if (count++ == 200)
		{
			// For testing, load up the default scenario

			//sprintf_s(simmgr_shm->status.scenario.active, STR_SIZE, "default");
			//simmgr_shm->status.scenario.record = 0;
			//start_scenario();
		}
		if (_kbhit())
		{
			cc = _getch();
			if (cc == 'q' || cc == 'Q')
			{
				break;
			}
		}
		simmgrRun();
	}
	printf("Exiting\n" );
	stopPHPServer();
}

void
simmgrInitialize(void)
{
	char* ptr;
	typedef void (*SignalHandlerPointer)(int);

	SignalHandlerPointer previousHandler;
	previousHandler = signal(SIGABRT, SignalHandler);
	previousHandler = signal(SIGFPE, SignalHandler);
	previousHandler = signal(SIGILL, SignalHandler);
	previousHandler = signal(SIGINT, SignalHandler);
	previousHandler = signal(SIGSEGV, SignalHandler);
	previousHandler = signal(SIGTERM, SignalHandler);

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

	simmgr_shm->instructor.sema = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex

	vs_iiLockTaken = 0;

	// instructor/scenario
	sprintf_s(simmgr_shm->instructor.scenario.active, STR_SIZE, "%s", "");
	sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "");
	simmgr_shm->instructor.scenario.record = -1;

	// Log File
	simmgr_shm->logfile.sema = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex

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

	timer_start(hrcheck_handler, 10 );
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
	simmgr_shm->status.respiration.chest_movement = 1;
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
	sprintf_s(simmgr_shm->status.telesim.vid[0].name, STR_SIZE, "%s", "none");
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
	sprintf_s(simmgr_shm->instructor.general.temperature_units, sizeof(simmgr_shm->instructor.general.temperature_units), "%s", "");

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
ULONGLONG breathLog[BREATH_LOG_LEN] = { 0, };
ULONGLONG breathLogNext = 0;

#define BREATH_LOG_STATE_IDLE	0
#define BREATH_LOG_STATE_DETECT	1
int breathLogState = BREATH_LOG_STATE_IDLE;

ULONGLONG breathLogLastNatural = 0;	// breathCount, last natural
ULONGLONG breathLogLastManual = 0;	// manual_count, last manual
ULONGLONG breathLogLast = 0;			// Time of last breath

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
	ULONGLONG now = simmgr_shm->server.msec_time; //  time(NULL);	// Current sec time
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
	ULONGLONG now = simmgr_shm->server.msec_time; //  time(NULL);	// Current sec time
	ULONGLONG prev;
	int breaths;
	ULONGLONG totalTime;
	ULONGLONG lastTime;
	ULONGLONG firstTime;
	ULONGLONG diff;
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

ULONGLONG cprLast = 0;
ULONGLONG cprRunTime = 0;
ULONGLONG cprDuration = 2000;

void
cpr_check(void)
{
	ULONGLONG now = simmgr_shm->server.msec_time;
	ULONGLONG cprCurrent = simmgr_shm->status.cpr.last;

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
ULONGLONG shockLast = 0;
ULONGLONG shockStartTime = 0;
ULONGLONG shockDuration = 2000;

void
shock_check(void)
{
	ULONGLONG now = simmgr_shm->server.msec_time;
	ULONGLONG shockCurrent = simmgr_shm->status.defibrillation.last;

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
ULONGLONG hrLog[HR_LOG_LEN] = { 0, };
ULONGLONG hrLogNext = 0;
ULONGLONG hrLogLastNatural = 0;	// beatCount, last natural
ULONGLONG hrLogLastVPC = 0;	// VPC count, last VPC

#define HR_LOG_DELAY	(40)
int hrLogDelay = 0;

#define HR_LOG_CHANGE_LOOPS	50	// hr_check is called every 5 msec. So this will cause a recalc every 250 msec
int hrLogReportLoops = 0;

void hrcheck_handler(void )     // current system time  )    // additional information 
{
	ULONGLONG now; // Current msec time
	ULONGLONG prev;
	int beats;
	ULONGLONG totalTime;
	ULONGLONG lastTime;
	ULONGLONG firstTime;
	ULONGLONG diff;
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
	static int reports = 0;
	hrCheckCount++;

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
 * msec_time_update
*/
ULONGLONG
msec_time_update(void)
{
	ULONGLONG msec;

	msec = GetTickCount64();

	simmgr_shm->server.msec_time = msec;
	// printf("Tick %ull\n", simmgr_shm->server.msec_time);
	return (msec);
}
/*
 * time_update
 *
 * Get the localtime and write it as a string to the SHM data
 */
int last_time_sec = -1;

void
time_update(void)
{
	int hour;
	int min;
	int elapsedTimeSeconds;
	int seconds;
	int sec;
	double temperature;
	char buf[BUF_SIZE];

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

	if ((scenario_state == ScenarioState::ScenarioRunning) ||
		(scenario_state == ScenarioState::ScenarioPaused))
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
		((scenario_state == ScenarioState::ScenarioRunning) ||
			(scenario_state == ScenarioState::ScenarioPaused)))
	{

		sprintf_s(buf, BUF_SIZE, "Scenario: MAX Scenario Runtime exceeded. Terminating.");
		simlog_entry(buf);
		printf("Scenario: MAX Scenario Runtime exceeded. Terminating.\n");
		printf("Now:   %lld\n", now);
		printf("Start: %lld\n", scenario_start_time);

		printf("Elapsed Time %d\n", elapsedTimeSeconds);
		takeInstructorLock();
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "Terminate");
		releaseInstructorLock();
	}
	else if (scenario_state == ScenarioState::ScenarioRunning)
	{
		sec = (int)(simmgr_shm->status.scenario.elapsed_msec_scenario / 1000);
		min = (sec / 60);
		hour = min / 60;
		sprintf_s(simmgr_shm->status.scenario.runtimeScenario, STR_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);

		sec = (int)(simmgr_shm->status.scenario.elapsed_msec_scene / 1000);
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
	else if (scenario_state == ScenarioState::ScenarioStopped)
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


int
setTrend(struct trend* trend, int end, int current, int duration)
{
	double diff;

	trend->end = (double)end;
	diff = (double)abs(end - current);

	if ((duration > 0) && (diff > 0))
	{
		trend->current = (double)current;
		trend->changePerSecond = diff / duration;
		trend->nextTime = time(0) + 1;
	}
	else
	{
		trend->current = end;
		trend->changePerSecond = 0;
		trend->nextTime = 0;
	}
	return ((int)trend->current);
}

int
trendProcess(struct trend* trend)
{
	time_t now;
	double newval;
	int rval;

	now = time(0);

	if (trend->nextTime && (trend->nextTime <= now))
	{
		if (trend->end > trend->current)
		{
			newval = trend->current + trend->changePerSecond;
			if (newval > trend->end)
			{
				newval = trend->end;
			}
		}
		else
		{
			newval = trend->current - trend->changePerSecond;
			if (newval < trend->end)
			{
				newval = trend->end;
			}
		}
		trend->current = newval;
		if (trend->current == trend->end)
		{
			trend->nextTime = 0;
		}
		else
		{
			trend->nextTime = now + 1;
		}
	}
	rval = (int)round(trend->current);
	return (rval);
}

bool
isRhythmPulsed(char* rhythm)
{
	if ((strcmp(rhythm, "asystole") == 0) ||
		(strcmp(rhythm, "vfib") == 0))
	{
		return (false);
	}
	else
	{
		return (true);
	}
}


void
setRespirationPeriods(int oldRate, int newRate)
{
	int period;

	//if ( oldRate != newRate )
	{
		if (newRate > 0)
		{
			simmgr_shm->status.respiration.rate = newRate;
			period = (1000 * 60) / newRate;	// Period in msec from rate per minute
			if (period > 10000)
			{
				period = 10000;
			}
			period = (period * 7) / 10;	// Use 70% of the period for duration calculations
			simmgr_shm->status.respiration.inhalation_duration = period / 2;
			simmgr_shm->status.respiration.exhalation_duration = period - simmgr_shm->status.respiration.inhalation_duration;
		}
		else
		{
			simmgr_shm->status.respiration.rate = 0;
			simmgr_shm->status.respiration.inhalation_duration = 0;
			simmgr_shm->status.respiration.exhalation_duration = 0;
		}
	}
}

/*
 * Scan commands from Initiator Interface
 *
 * Reads II commands and changes operating parameters
 *
 * Note: Events are added to the Event List directly by the source initiators and read
 * by the scenario process. Events are not handled here.
 */
int
scan_commands(void)
{
	int sts;
	int trycount;
	int oldRate;
	int newRate;
	bool currentIsPulsed;
	bool newIsPulsed;
	int v;
	char buf[BUF_SIZE];

	// Lock the command interface before processing commands
	trycount = 0;

	if (takeInstructorLock())
	{
		vs_iiLockTaken = 1;
	}
	// Check for instructor commands

	// Scenario
	if (simmgr_shm->instructor.scenario.record >= 0)
	{
		simmgr_shm->status.scenario.record = simmgr_shm->instructor.scenario.record;
		simmgr_shm->instructor.scenario.record = -1;
	}

	if (strlen(simmgr_shm->instructor.scenario.state) > 0)
	{
		strToLower(simmgr_shm->instructor.scenario.state);

		sprintf_s(msg_buf, BUF_SIZE, "State Request: \"%s\" Current \"%s\" State %d",
			simmgr_shm->instructor.scenario.state,
			simmgr_shm->status.scenario.state,
			scenario_state);
		log_message("", msg_buf);

		if (strcmp(simmgr_shm->instructor.scenario.state, "paused") == 0)
		{
			printf("paused\n");
			if (scenario_state == ScenarioState::ScenarioRunning)
			{
				updateScenarioState(ScenarioState::ScenarioPaused);
			}
		}
		else if (strcmp(simmgr_shm->instructor.scenario.state, "running") == 0)
		{
			printf("running\n");
			if (scenario_state == ScenarioState::ScenarioPaused)
			{
				printf("Calling updateScenarioState(Running)\n");
				updateScenarioState(ScenarioState::ScenarioRunning);
			}
			else if (scenario_state == ScenarioState::ScenarioStopped)
			{
				printf("Calling start_scenario\n");
				scenario_start_time = time(nullptr);
				sts = start_scenario();
			}
			else if(scenario_state == ScenarioState::ScenarioRunning)
			{
				printf("Error: scenario_state is Running \n");
			}
			else if (scenario_state == ScenarioState::ScenarioTerminate)
			{
				printf("Error: scenario_state is Terminate \n");
			}
		}
		else if (strcmp(simmgr_shm->instructor.scenario.state, "terminate") == 0)
		{
			printf("terminated\n");
			if (scenario_state != ScenarioState::ScenarioTerminate)
			{
				updateScenarioState(ScenarioState::ScenarioTerminate);
			}
		}
		else if (strcmp(simmgr_shm->instructor.scenario.state, "stopped") == 0)
		{
			printf("stopped\n");
			if (scenario_state != ScenarioState::ScenarioStopped)
			{
				updateScenarioState(ScenarioState::ScenarioStopped);
			}
		}
		else
		{
			printf("unknown\n");
		}
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "");
	}
	if (strlen(simmgr_shm->instructor.scenario.active) > 0)
	{
		sprintf_s(msg_buf, BUF_SIZE,"Set Active: %s State %d", simmgr_shm->instructor.scenario.active, scenario_state);
		log_message("", msg_buf);
		switch (scenario_state)
		{
		case ScenarioState::ScenarioTerminate:
		default:
			break;
		case ScenarioState::ScenarioStopped:
			sprintf_s(simmgr_shm->status.scenario.active, STR_SIZE, "%s", simmgr_shm->instructor.scenario.active);
			break;
		}
		sprintf_s(simmgr_shm->instructor.scenario.active, STR_SIZE, "%s", "");
	}

	// Cardiac
	if (strlen(simmgr_shm->instructor.cardiac.rhythm) > 0)
	{
		if (strcmp(simmgr_shm->status.cardiac.rhythm, simmgr_shm->instructor.cardiac.rhythm) != 0)
		{
			// When changing to pulseless rhythm, the rate will be set to zero.
			newIsPulsed = isRhythmPulsed(simmgr_shm->instructor.cardiac.rhythm);
			if (newIsPulsed == false)
			{
				simmgr_shm->instructor.cardiac.rate = 0;
				simmgr_shm->instructor.cardiac.transfer_time = 0;
			}
			else
			{
				// When changing from a pulseless rhythm to a pulse rhythm, the rate will be set to 100
				// This can be overridden in the command by setting the rate explicitly
				currentIsPulsed = isRhythmPulsed(simmgr_shm->status.cardiac.rhythm);
				if ((currentIsPulsed == false) && (newIsPulsed == true))
				{
					if (simmgr_shm->instructor.cardiac.rate < 0)
					{
						simmgr_shm->instructor.cardiac.rate = 100;
						simmgr_shm->instructor.cardiac.transfer_time = 0;
					}
				}
			}
			sprintf_s(simmgr_shm->status.cardiac.rhythm, STR_SIZE, "%s", simmgr_shm->instructor.cardiac.rhythm);
			sprintf_s(buf, BUF_SIZE, "Setting: %s: %s", "Cardiac Rhythm", simmgr_shm->instructor.cardiac.rhythm);
			simlog_entry(buf);
		}
		sprintf_s(simmgr_shm->instructor.cardiac.rhythm, STR_SIZE, "%s", "");

	}
	if (simmgr_shm->instructor.cardiac.rate >= 0)
	{
		currentIsPulsed = isRhythmPulsed(simmgr_shm->status.cardiac.rhythm);
		if (currentIsPulsed == true)
		{
			if (simmgr_shm->instructor.cardiac.rate != simmgr_shm->status.cardiac.rate)
			{
				simmgr_shm->status.cardiac.rate = setTrend(&cardiacTrend,
					simmgr_shm->instructor.cardiac.rate,
					simmgr_shm->status.cardiac.rate,
					simmgr_shm->instructor.cardiac.transfer_time);
				if (simmgr_shm->instructor.cardiac.transfer_time >= 0)
				{
					sprintf_s(buf, BUF_SIZE, "Setting: %s: %d time %d", "Cardiac Rate",simmgr_shm->instructor.cardiac.rate, simmgr_shm->instructor.cardiac.transfer_time);
				}
				else
				{
					sprintf_s(buf, BUF_SIZE, "Setting: %s: %d", "Cardiac Rate", simmgr_shm->instructor.cardiac.rate);
				}
				simlog_entry(buf);
			}
		}
		else
		{
			if (simmgr_shm->instructor.cardiac.rate > 0)
			{
				sprintf_s(buf, BUF_SIZE, "Setting: %s: %d", "Cardiac Rate cannot be set while in pulseless rhythm", simmgr_shm->instructor.cardiac.rate);
				simlog_entry(buf);
			}
			else
			{
				simmgr_shm->status.cardiac.rate = setTrend(&cardiacTrend,
					0,
					simmgr_shm->status.cardiac.rate,
					0);
			}
		}
		simmgr_shm->instructor.cardiac.rate = -1;
	}
	if (simmgr_shm->instructor.cardiac.nibp_rate >= 0)
	{
		if (simmgr_shm->status.cardiac.nibp_rate != simmgr_shm->instructor.cardiac.nibp_rate)
		{
			simmgr_shm->status.cardiac.nibp_rate = simmgr_shm->instructor.cardiac.nibp_rate;
			sprintf_s(buf, BUF_SIZE, "Setting: %s: %d", "NIBP Rate", simmgr_shm->instructor.cardiac.rate);
			simlog_entry(buf);
		}
		simmgr_shm->instructor.cardiac.nibp_rate = -1;
	}
	if (simmgr_shm->instructor.cardiac.nibp_read >= 0)
	{
		if (simmgr_shm->status.cardiac.nibp_read != simmgr_shm->instructor.cardiac.nibp_read)
		{
			simmgr_shm->status.cardiac.nibp_read = simmgr_shm->instructor.cardiac.nibp_read;
		}
		simmgr_shm->instructor.cardiac.nibp_read = -1;
	}
	if (simmgr_shm->instructor.cardiac.nibp_linked_hr >= 0)
	{
		if (simmgr_shm->status.cardiac.nibp_linked_hr != simmgr_shm->instructor.cardiac.nibp_linked_hr)
		{
			simmgr_shm->status.cardiac.nibp_linked_hr = simmgr_shm->instructor.cardiac.nibp_linked_hr;
		}
		simmgr_shm->instructor.cardiac.nibp_linked_hr = -1;
	}
	if (simmgr_shm->instructor.cardiac.nibp_freq >= 0)
	{
		if (simmgr_shm->status.cardiac.nibp_freq != simmgr_shm->instructor.cardiac.nibp_freq)
		{
			simmgr_shm->status.cardiac.nibp_freq = simmgr_shm->instructor.cardiac.nibp_freq;
			if (nibp_state == NibpState::NibpWaiting) // Cancel current wait and allow reset to new rate
			{
				nibp_state = NibpState::NibpIdle;
			}
		}
		simmgr_shm->instructor.cardiac.nibp_freq = -1;
	}
	if (strlen(simmgr_shm->instructor.cardiac.pwave) > 0)
	{
		sprintf_s(simmgr_shm->status.cardiac.pwave, STR_SIZE, "%s", simmgr_shm->instructor.cardiac.pwave);
		sprintf_s(simmgr_shm->instructor.cardiac.pwave, STR_SIZE, "%s", "");
	}
	if (simmgr_shm->instructor.cardiac.pr_interval >= 0)
	{
		simmgr_shm->status.cardiac.pr_interval = simmgr_shm->instructor.cardiac.pr_interval;
		simmgr_shm->instructor.cardiac.pr_interval = -1;
	}
	if (simmgr_shm->instructor.cardiac.qrs_interval >= 0)
	{
		simmgr_shm->status.cardiac.qrs_interval = simmgr_shm->instructor.cardiac.qrs_interval;
		simmgr_shm->instructor.cardiac.qrs_interval = -1;
	}
	if (simmgr_shm->instructor.cardiac.qrs_interval >= 0)
	{
		simmgr_shm->status.cardiac.qrs_interval = simmgr_shm->instructor.cardiac.qrs_interval;
		simmgr_shm->instructor.cardiac.qrs_interval = -1;
	}
	if (simmgr_shm->instructor.cardiac.bps_sys >= 0)
	{
		simmgr_shm->status.cardiac.bps_sys = setTrend(&sysTrend,
			simmgr_shm->instructor.cardiac.bps_sys,
			simmgr_shm->status.cardiac.bps_sys,
			simmgr_shm->instructor.cardiac.transfer_time);
		simmgr_shm->instructor.cardiac.bps_sys = -1;
	}
	if (simmgr_shm->instructor.cardiac.bps_dia >= 0)
	{
		simmgr_shm->status.cardiac.bps_dia = setTrend(&diaTrend,
			simmgr_shm->instructor.cardiac.bps_dia,
			simmgr_shm->status.cardiac.bps_dia,
			simmgr_shm->instructor.cardiac.transfer_time);
		simmgr_shm->instructor.cardiac.bps_dia = -1;
	}
	if (simmgr_shm->instructor.cardiac.pea >= 0)
	{
		simmgr_shm->status.cardiac.pea = simmgr_shm->instructor.cardiac.pea;
		simmgr_shm->instructor.cardiac.pea = -1;
	}
	if (simmgr_shm->instructor.cardiac.right_dorsal_pulse_strength >= 0)
	{
		simmgr_shm->status.cardiac.right_dorsal_pulse_strength = simmgr_shm->instructor.cardiac.right_dorsal_pulse_strength;
		simmgr_shm->instructor.cardiac.right_dorsal_pulse_strength = -1;
	}
	if (simmgr_shm->instructor.cardiac.right_femoral_pulse_strength >= 0)
	{
		simmgr_shm->status.cardiac.right_femoral_pulse_strength = simmgr_shm->instructor.cardiac.right_femoral_pulse_strength;
		simmgr_shm->instructor.cardiac.right_femoral_pulse_strength = -1;
	}
	if (simmgr_shm->instructor.cardiac.left_dorsal_pulse_strength >= 0)
	{
		simmgr_shm->status.cardiac.left_dorsal_pulse_strength = simmgr_shm->instructor.cardiac.left_dorsal_pulse_strength;
		simmgr_shm->instructor.cardiac.left_dorsal_pulse_strength = -1;
	}
	if (simmgr_shm->instructor.cardiac.left_femoral_pulse_strength >= 0)
	{
		simmgr_shm->status.cardiac.left_femoral_pulse_strength = simmgr_shm->instructor.cardiac.left_femoral_pulse_strength;
		simmgr_shm->instructor.cardiac.left_femoral_pulse_strength = -1;
	}
	if (simmgr_shm->instructor.cardiac.vpc_freq >= 0)
	{
		simmgr_shm->status.cardiac.vpc_freq = simmgr_shm->instructor.cardiac.vpc_freq;
		simmgr_shm->instructor.cardiac.vpc_freq = -1;
	}
	/*
	if ( simmgr_shm->instructor.cardiac.vpc_delay >= 0 )
	{
		simmgr_shm->status.cardiac.vpc_delay = simmgr_shm->instructor.cardiac.vpc_delay;
		simmgr_shm->instructor.cardiac.vpc_delay = -1;
	}
	*/
	if (strlen(simmgr_shm->instructor.cardiac.vpc) > 0)
	{
		sprintf_s(simmgr_shm->status.cardiac.vpc, STR_SIZE, "%s", simmgr_shm->instructor.cardiac.vpc);
		sprintf_s(simmgr_shm->instructor.cardiac.vpc, STR_SIZE, "%s", "");
		switch (simmgr_shm->status.cardiac.vpc[0])
		{
		case '1':
			simmgr_shm->status.cardiac.vpc_type = 1;
			break;
		case '2':
			simmgr_shm->status.cardiac.vpc_type = 2;
			break;
		default:
			simmgr_shm->status.cardiac.vpc_type = 0;
			break;
		}
		switch (simmgr_shm->status.cardiac.vpc[2])
		{
		case '1':
			simmgr_shm->status.cardiac.vpc_count = 1;
			break;
		case '2':
			simmgr_shm->status.cardiac.vpc_count = 2;
			break;
		case '3':
			simmgr_shm->status.cardiac.vpc_count = 3;
			break;
		default:
			simmgr_shm->status.cardiac.vpc_count = 0;
			simmgr_shm->status.cardiac.vpc_type = 0;
			break;
		}
	}
	if (strlen(simmgr_shm->instructor.cardiac.vfib_amplitude) > 0)
	{
		sprintf_s(simmgr_shm->status.cardiac.vfib_amplitude, STR_SIZE, "%s", simmgr_shm->instructor.cardiac.vfib_amplitude);
		sprintf_s(simmgr_shm->instructor.cardiac.vfib_amplitude, STR_SIZE, "%s", "");
	}
	if (strlen(simmgr_shm->instructor.cardiac.heart_sound) > 0)
	{
		sprintf_s(simmgr_shm->status.cardiac.heart_sound, STR_SIZE, "%s", simmgr_shm->instructor.cardiac.heart_sound);
		sprintf_s(simmgr_shm->instructor.cardiac.heart_sound, STR_SIZE, "%s", "");
	}
	if (simmgr_shm->instructor.cardiac.heart_sound_volume >= 0)
	{
		simmgr_shm->status.cardiac.heart_sound_volume = simmgr_shm->instructor.cardiac.heart_sound_volume;
		simmgr_shm->instructor.cardiac.heart_sound_volume = -1;
	}
	if (simmgr_shm->instructor.cardiac.heart_sound_mute >= 0)
	{
		simmgr_shm->status.cardiac.heart_sound_mute = simmgr_shm->instructor.cardiac.heart_sound_mute;
		simmgr_shm->instructor.cardiac.heart_sound_mute = -1;
	}

	if (simmgr_shm->instructor.cardiac.ecg_indicator >= 0)
	{
		if (simmgr_shm->status.cardiac.ecg_indicator != simmgr_shm->instructor.cardiac.ecg_indicator)
		{
			simmgr_shm->status.cardiac.ecg_indicator = simmgr_shm->instructor.cardiac.ecg_indicator;
			sprintf_s(buf, BUF_SIZE, "Probe: %s %s", "ECG", (simmgr_shm->status.cardiac.ecg_indicator == 1 ? "Attached" : "Removed"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.cardiac.ecg_indicator = -1;
	}
	if (simmgr_shm->instructor.cardiac.bp_cuff >= 0)
	{
		if (simmgr_shm->status.cardiac.bp_cuff != simmgr_shm->instructor.cardiac.bp_cuff)
		{
			simmgr_shm->status.cardiac.bp_cuff = simmgr_shm->instructor.cardiac.bp_cuff;
			sprintf_s(buf, BUF_SIZE, "Probe: %s %s", "BP Cuff", (simmgr_shm->status.cardiac.bp_cuff == 1 ? "Attached" : "Removed"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.cardiac.bp_cuff = -1;
	}
	if (simmgr_shm->instructor.cardiac.arrest >= 0)
	{
		if (simmgr_shm->status.cardiac.arrest != simmgr_shm->instructor.cardiac.arrest)
		{
			simmgr_shm->status.cardiac.arrest = simmgr_shm->instructor.cardiac.arrest;
			sprintf_s(buf, BUF_SIZE, "Setting: %s %s", "Arrest", (simmgr_shm->status.cardiac.arrest == 1 ? "Start" : "Stop"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.cardiac.arrest = -1;
	}
	simmgr_shm->instructor.cardiac.transfer_time = -1;

	// Respiration
	if (strlen(simmgr_shm->instructor.respiration.left_lung_sound) > 0)
	{
		sprintf_s(simmgr_shm->status.respiration.left_lung_sound, STR_SIZE, "%s", simmgr_shm->instructor.respiration.left_lung_sound);
		sprintf_s(simmgr_shm->instructor.respiration.left_lung_sound, STR_SIZE, "%s", "");
	}
	if (strlen(simmgr_shm->instructor.respiration.right_lung_sound) > 0)
	{
		sprintf_s(simmgr_shm->status.respiration.right_lung_sound, STR_SIZE, "%s", simmgr_shm->instructor.respiration.right_lung_sound);
		sprintf_s(simmgr_shm->instructor.respiration.right_lung_sound, STR_SIZE, "%s", "");
	}
	/*
	if ( simmgr_shm->instructor.respiration.inhalation_duration >= 0 )
	{
		simmgr_shm->status.respiration.inhalation_duration = simmgr_shm->instructor.respiration.inhalation_duration;
		simmgr_shm->instructor.respiration.inhalation_duration = -1;
	}
	if ( simmgr_shm->instructor.respiration.exhalation_duration >= 0 )
	{
		simmgr_shm->status.respiration.exhalation_duration = simmgr_shm->instructor.respiration.exhalation_duration;
		simmgr_shm->instructor.respiration.exhalation_duration = -1;
	}
	*/
	if (simmgr_shm->instructor.respiration.left_lung_sound_volume >= 0)
	{
		simmgr_shm->status.respiration.left_lung_sound_volume = simmgr_shm->instructor.respiration.left_lung_sound_volume;
		simmgr_shm->instructor.respiration.left_lung_sound_volume = -1;
	}
	if (simmgr_shm->instructor.respiration.left_lung_sound_mute >= 0)
	{
		simmgr_shm->status.respiration.left_lung_sound_mute = simmgr_shm->instructor.respiration.left_lung_sound_mute;
		simmgr_shm->instructor.respiration.left_lung_sound_mute = -1;
	}
	if (simmgr_shm->instructor.respiration.right_lung_sound_volume >= 0)
	{
		simmgr_shm->status.respiration.right_lung_sound_volume = simmgr_shm->instructor.respiration.right_lung_sound_volume;
		simmgr_shm->instructor.respiration.right_lung_sound_volume = -1;
	}
	if (simmgr_shm->instructor.respiration.right_lung_sound_mute >= 0)
	{
		simmgr_shm->status.respiration.right_lung_sound_mute = simmgr_shm->instructor.respiration.right_lung_sound_mute;
		simmgr_shm->instructor.respiration.right_lung_sound_mute = -1;
	}
	if (simmgr_shm->instructor.respiration.rate >= 0)
	{
		sprintf_s(msg_buf, BUF_SIZE,"Setting: Resp Rate = %d -> %d : %d", simmgr_shm->status.respiration.rate, simmgr_shm->instructor.respiration.rate, simmgr_shm->instructor.respiration.transfer_time);
		log_message("", msg_buf);
		simmgr_shm->status.respiration.rate = setTrend(&respirationTrend,
			simmgr_shm->instructor.respiration.rate,
			simmgr_shm->status.respiration.rate,
			simmgr_shm->instructor.respiration.transfer_time);
		if (simmgr_shm->instructor.respiration.transfer_time == 0)
		{
			setRespirationPeriods(simmgr_shm->status.respiration.rate, simmgr_shm->instructor.respiration.rate);
		}
		simmgr_shm->instructor.respiration.rate = -1;
	}
	if (simmgr_shm->instructor.respiration.spo2 >= 0)
	{
		simmgr_shm->status.respiration.spo2 = setTrend(&spo2Trend,
			simmgr_shm->instructor.respiration.spo2,
			simmgr_shm->status.respiration.spo2,
			simmgr_shm->instructor.respiration.transfer_time);
		simmgr_shm->instructor.respiration.spo2 = -1;
	}

	if (simmgr_shm->instructor.respiration.etco2 >= 0)
	{
		simmgr_shm->status.respiration.etco2 = setTrend(&etco2Trend,
			simmgr_shm->instructor.respiration.etco2,
			simmgr_shm->status.respiration.etco2,
			simmgr_shm->instructor.respiration.transfer_time);
		simmgr_shm->instructor.respiration.etco2 = -1;
	}
	if (simmgr_shm->instructor.respiration.etco2_indicator >= 0)
	{
		if (simmgr_shm->status.respiration.etco2_indicator != simmgr_shm->instructor.respiration.etco2_indicator)
		{
			simmgr_shm->status.respiration.etco2_indicator = simmgr_shm->instructor.respiration.etco2_indicator;
			sprintf_s(buf, BUF_SIZE, "Probe: %s %s", "ETCO2", (simmgr_shm->status.respiration.etco2_indicator == 1 ? "Attached" : "Removed"));
			simlog_entry(buf);
		}

		simmgr_shm->instructor.respiration.etco2_indicator = -1;
	}
	if (simmgr_shm->instructor.respiration.spo2_indicator >= 0)
	{
		if (simmgr_shm->status.respiration.spo2_indicator != simmgr_shm->instructor.respiration.spo2_indicator)
		{
			simmgr_shm->status.respiration.spo2_indicator = simmgr_shm->instructor.respiration.spo2_indicator;
			sprintf_s(buf, BUF_SIZE, "Probe: %s %s", "SPO2", (simmgr_shm->status.respiration.spo2_indicator == 1 ? "Attached" : "Removed"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.respiration.spo2_indicator = -1;
	}
	if (simmgr_shm->instructor.respiration.chest_movement >= 0)
	{
		if (simmgr_shm->status.respiration.chest_movement != simmgr_shm->instructor.respiration.chest_movement)
		{
			simmgr_shm->status.respiration.chest_movement = simmgr_shm->instructor.respiration.chest_movement;
		}
		simmgr_shm->instructor.respiration.chest_movement = -1;
	}
	if (simmgr_shm->instructor.respiration.manual_breath >= 0)
	{
		simmgr_shm->status.respiration.manual_count++;
		simmgr_shm->instructor.respiration.manual_breath = -1;
	}
	simmgr_shm->instructor.respiration.transfer_time = -1;

	// General
	if (simmgr_shm->instructor.general.temperature >= 0)
	{
		simmgr_shm->status.general.temperature = setTrend(&tempTrend,
			simmgr_shm->instructor.general.temperature,
			simmgr_shm->status.general.temperature,
			simmgr_shm->instructor.general.transfer_time);
		simmgr_shm->instructor.general.temperature = -1;
	}
	if (strlen(simmgr_shm->instructor.general.temperature_units) > 0)
	{
		if (simmgr_shm->instructor.general.temperature_units[0] != simmgr_shm->status.general.temperature_units[0])
		{
			if (simmgr_shm->instructor.general.temperature_units[0] == 'F' ||
				simmgr_shm->instructor.general.temperature_units[0] == 'C')
			{
				sprintf_s(simmgr_shm->status.general.temperature_units, STR_SIZE, "%s",
					simmgr_shm->instructor.general.temperature_units);
			}
			sprintf_s(simmgr_shm->instructor.general.temperature_units, STR_SIZE, "%s", "");
		}
	}
	if (simmgr_shm->instructor.general.temperature_enable >= 0)
	{
		if (simmgr_shm->status.general.temperature_enable != simmgr_shm->instructor.general.temperature_enable)
		{
			simmgr_shm->status.general.temperature_enable = simmgr_shm->instructor.general.temperature_enable;
			sprintf_s(buf, BUF_SIZE, "Probe: %s %s", "Temp", (simmgr_shm->status.general.temperature_enable == 1 ? "Attached" : "Removed"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.general.temperature_enable = -1;
	}
	simmgr_shm->instructor.general.transfer_time = -1;
	if (strlen(simmgr_shm->instructor.general.clockStart) > 0)
	{
		struct tm tm;
		time_t now;
		now = std::time(nullptr);
		localtime_s(&tm, &now);

		sprintf_s(simmgr_shm->status.general.clockStart, STR_SIZE, "%s", simmgr_shm->instructor.general.clockStart);
		sprintf_s(simmgr_shm->instructor.general.clockStart, STR_SIZE, "%s", "");
		sprintf_s(simmgr_shm->status.general.clockStart, STR_SIZE, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
		sprintf_s(buf, BUF_SIZE, "%s %02d %02d %02d", "time returned", tm.tm_hour, tm.tm_min, tm.tm_sec);
		log_message("", buf);

		simmgr_shm->status.general.clockStartSec = (tm.tm_hour * 60 * 60) + (tm.tm_min * 60) + tm.tm_sec;
	}
	// vocals
	if (strlen(simmgr_shm->instructor.vocals.filename) > 0)
	{
		sprintf_s(simmgr_shm->status.vocals.filename, STR_SIZE, "%s", simmgr_shm->instructor.vocals.filename);
		sprintf_s(simmgr_shm->instructor.vocals.filename, STR_SIZE, "%s", "");
	}
	if (simmgr_shm->instructor.vocals.repeat >= 0)
	{
		simmgr_shm->status.vocals.repeat = simmgr_shm->instructor.vocals.repeat;
		simmgr_shm->instructor.vocals.repeat = -1;
	}
	if (simmgr_shm->instructor.vocals.volume >= 0)
	{
		simmgr_shm->status.vocals.volume = simmgr_shm->instructor.vocals.volume;
		simmgr_shm->instructor.vocals.volume = -1;
	}
	if (simmgr_shm->instructor.vocals.play >= 0)
	{
		simmgr_shm->status.vocals.play = simmgr_shm->instructor.vocals.play;
		simmgr_shm->instructor.vocals.play = -1;
	}
	if (simmgr_shm->instructor.vocals.mute >= 0)
	{
		simmgr_shm->status.vocals.mute = simmgr_shm->instructor.vocals.mute;
		simmgr_shm->instructor.vocals.mute = -1;
	}

	// media
	if (strlen(simmgr_shm->instructor.media.filename) > 0)
	{
		sprintf_s(simmgr_shm->status.media.filename, STR_SIZE, "%s", simmgr_shm->instructor.media.filename);
		sprintf_s(simmgr_shm->instructor.media.filename, STR_SIZE, "%s", "");
	}
	if (simmgr_shm->instructor.media.play != -1)
	{
		simmgr_shm->status.media.play = simmgr_shm->instructor.media.play;
		simmgr_shm->instructor.media.play = -1;
	}
	// telesim
	if (simmgr_shm->instructor.telesim.enable >= 0)
	{
		if (simmgr_shm->status.telesim.enable != simmgr_shm->instructor.telesim.enable)
		{
			simmgr_shm->status.telesim.enable = simmgr_shm->instructor.telesim.enable;
			sprintf_s(buf, BUF_SIZE, "TeleSim Mode: %s", (simmgr_shm->status.telesim.enable == 1 ? "Enabled" : "Disabled"));
			simlog_entry(buf);
		}
		simmgr_shm->instructor.telesim.enable = -1;
	}
	for (v = 0; v < TSIM_WINDOWS; v++)
	{
		if (strlen(simmgr_shm->instructor.telesim.vid[v].name) > 0)
		{
			sprintf_s(simmgr_shm->status.telesim.vid[v].name, STR_SIZE, "%s", simmgr_shm->instructor.telesim.vid[v].name);
			sprintf_s(simmgr_shm->instructor.telesim.vid[v].name, STR_SIZE, "%s", "");
		}
		if (simmgr_shm->instructor.telesim.vid[v].next > 0 &&
			simmgr_shm->instructor.telesim.vid[v].next != simmgr_shm->status.telesim.vid[v].next)
		{
			sprintf_s(buf, BUF_SIZE, "TeleSim vid %d Next %d:%d", v, simmgr_shm->status.telesim.vid[v].next, simmgr_shm->instructor.telesim.vid[v].next);
			simlog_entry(buf);
			simmgr_shm->status.telesim.vid[v].command = simmgr_shm->instructor.telesim.vid[v].command;
			simmgr_shm->status.telesim.vid[v].param = simmgr_shm->instructor.telesim.vid[v].param;
			simmgr_shm->status.telesim.vid[v].next = simmgr_shm->instructor.telesim.vid[v].next;
		}
	}
	// CPR
	if (simmgr_shm->instructor.cpr.compression >= 0)
	{
		simmgr_shm->status.cpr.compression = simmgr_shm->instructor.cpr.compression;
		if (simmgr_shm->status.cpr.compression)
		{
			simmgr_shm->status.cpr.last = simmgr_shm->server.msec_time;
			simmgr_shm->status.cpr.running = 1;
		}
		simmgr_shm->instructor.cpr.compression = -1;
	}
	// Defibbrilation
	if (simmgr_shm->instructor.defibrillation.shock >= 0)
	{
		if (simmgr_shm->instructor.defibrillation.shock > 0)
		{
			simmgr_shm->status.defibrillation.last += 1;
		}
		simmgr_shm->instructor.defibrillation.shock = -1;
	}
	if (simmgr_shm->instructor.defibrillation.energy >= 0)
	{
		simmgr_shm->status.defibrillation.energy = simmgr_shm->instructor.defibrillation.energy;
		simmgr_shm->instructor.defibrillation.energy = -1;
	}
	// Release the MUTEX
	releaseInstructorLock();
	vs_iiLockTaken = 0;

	// Process the trends
	// We do this even if no scenario is running, to allow an instructor simple, manual control
	simmgr_shm->status.cardiac.rate = trendProcess(&cardiacTrend);
	simmgr_shm->status.cardiac.bps_sys = trendProcess(&sysTrend);
	simmgr_shm->status.cardiac.bps_dia = trendProcess(&diaTrend);
	oldRate = simmgr_shm->status.respiration.rate;
	newRate = trendProcess(&respirationTrend);
	setRespirationPeriods(oldRate, newRate);

	// simmgr_shm->status.respiration.awRR = simmgr_shm->status.respiration.rate;
	simmgr_shm->status.respiration.spo2 = trendProcess(&spo2Trend);
	simmgr_shm->status.respiration.etco2 = trendProcess(&etco2Trend);
	simmgr_shm->status.general.temperature = trendProcess(&tempTrend);

	// NIBP processing
	now = std::time(nullptr);
	switch (nibp_state)
	{
	case NibpState::NibpIdle:	// Not started or BP Cuff detached
		if (simmgr_shm->status.cardiac.bp_cuff > 0)
		{
			if (simmgr_shm->status.cardiac.nibp_read == 1)
			{
				// Manual Start - Go to Running for the run delay time
				nibp_run_complete_time = now + NIBP_RUN_TIME;
				nibp_state = NibpState::NibpRunning;
				snprintf(msg_buf, BUF_SIZE, "Action: NIBP Read Manual");
				lockAndComment(msg_buf);
			}
			else if (simmgr_shm->status.cardiac.nibp_freq != 0)
			{
				// Frequency set
				nibp_next_time = now + ((LONGLONG)simmgr_shm->status.cardiac.nibp_freq * 60);
				nibp_state = NibpState::NibpWaiting;
			}
		}
		break;
	case NibpState::NibpWaiting:
		if (simmgr_shm->status.cardiac.bp_cuff == 0) // Cuff removed
		{
			nibp_state = NibpState::NibpIdle;
		}
		else
		{
			if (simmgr_shm->status.cardiac.nibp_read == 1)
			{
				// Manual Override
				nibp_next_time = now;
			}
			if (nibp_next_time <= now)
			{
				nibp_run_complete_time = now + NIBP_RUN_TIME;
				nibp_state = NibpState::NibpRunning;

				snprintf(msg_buf, BUF_SIZE, "NIBP Read Periodic");
				lockAndComment(msg_buf);
				simmgr_shm->status.cardiac.nibp_read = 1;
			}
		}
		break;
	case NibpState::NibpRunning:
		if (simmgr_shm->status.cardiac.bp_cuff == 0) // Cuff removed
		{
			nibp_state = NibpState::NibpIdle;

			snprintf(msg_buf, BUF_SIZE, "NIBP Cuff removed while running");
			lockAndComment(msg_buf);
		}
		else
		{
			if (nibp_run_complete_time <= now)
			{
				int meanValue;

				simmgr_shm->status.cardiac.nibp_read = 0;

				meanValue = ((simmgr_shm->status.cardiac.bps_sys - simmgr_shm->status.cardiac.bps_dia) / 3) + simmgr_shm->status.cardiac.bps_dia;
				snprintf(msg_buf, BUF_SIZE, "NIBP %d/%d (%d)mmHg %d bpm",
					simmgr_shm->status.cardiac.bps_sys,
					simmgr_shm->status.cardiac.bps_dia,
					meanValue,
					simmgr_shm->status.cardiac.nibp_rate);
				lockAndComment(msg_buf);

				if (simmgr_shm->status.cardiac.nibp_freq != 0)
				{
					// Frequency set
					nibp_next_time = now + ((LONGLONG)simmgr_shm->status.cardiac.nibp_freq * 60);
					nibp_state = NibpState::NibpWaiting;

					sprintf_s(msg_buf, BUF_SIZE,"NibpState Change: Running to Waiting");
					log_message("", msg_buf);
				}
				else
				{
					nibp_state = NibpState::NibpIdle;
					sprintf_s(msg_buf, BUF_SIZE,"NibpState Change: Running to Idle");
					log_message("", msg_buf);
				}
			}
		}
		break;
	}
	/*
		if the BP Cuff is attached and we see nibp_read set, then
	*/
	if (scenario_state == ScenarioState::ScenarioTerminate)
	{
		if (simmgr_shm->logfile.active == 0)
		{
			updateScenarioState(ScenarioState::ScenarioTerminate);
		}
	}
	else if (scenario_state == ScenarioState::ScenarioStopped)
	{
		if (simmgr_shm->logfile.active == 0)
		{
			updateScenarioState(ScenarioState::ScenarioStopped);
		}
	}

	return (0);
}

int
start_scenario(void)
{
	char timeBuf[64];
	int fileCountBefore;
	int fileCountAfter;
	errno_t err;

	sprintf_s(c_msgbuf, BUF_SIZE, "Start Scenario Request: %s", simmgr_shm->status.scenario.active );
	log_message("", c_msgbuf);

	sprintf_s(sessionsPath, sizeof(sessionsPath), "/var/www/html/scenarios");
	resetAllParameters();

	if (simmgr_shm->status.scenario.record > 0)
	{
		fileCountBefore = getVideoFileCount();
		printf("File Count Before is %d\n", fileCountBefore);
		recordStartStop(1);
		while ((fileCountAfter = getVideoFileCount()) == fileCountBefore)
		{
			int cc;
			if (_kbhit())
			{
				cc = _getch();
				if (cc == 'q' || cc == 'Q')
				{
					printf("Stopped Waiting for Video File\n");
					break;
				}
			}
		}
	}

	simmgr_shm->status.scenario.startTime = std::time(nullptr);
	err = localtime_s(&simmgr_shm->status.scenario.tmStart, &simmgr_shm->status.scenario.startTime);
	if (err)
	{
		printf("Invalid Arg to localtime_s\n");
	}
	std::strftime(timeBuf, 60, "%c", &simmgr_shm->status.scenario.tmStart );

	(void)simlog_create();
	// start the new scenario
	start_task("scenario_main", scenario_main);
	
	sprintf_s(msg_buf, BUF_SIZE, "Start Scenario: %s Pid is %d", simmgr_shm->status.scenario.active, scenarioPid);
	log_message("", msg_buf);
	sprintf_s(simmgr_shm->status.scenario.active, STR_SIZE, "%s", simmgr_shm->status.scenario.active);

	sprintf_s(simmgr_shm->status.scenario.start, STR_SIZE, "%s", timeBuf);
	sprintf_s(simmgr_shm->status.scenario.runtimeAbsolute, STR_SIZE, "%s", "00:00:00");
	sprintf_s(simmgr_shm->status.scenario.runtimeScenario, STR_SIZE, "%s", "00:00:00");
	sprintf_s(simmgr_shm->status.scenario.runtimeScene, STR_SIZE, "%s", "00:00:00");

	updateScenarioState(ScenarioState::ScenarioRunning);

	return (0);
}


/*
 * checkEvents
 *
 * Scan through the event list and log any new events
 * Also scan comment list and and new ones to log file
 */

void
checkEvents(void)
{
	if ((lastEventLogged != simmgr_shm->eventListNext) ||
		(lastCommentLogged != simmgr_shm->commentListNext))
	{
		takeInstructorLock();
		while (lastEventLogged != simmgr_shm->eventListNext)
		{
			sprintf_s(msg_buf, BUF_SIZE, "Event: %s", simmgr_shm->eventList[lastEventLogged].eventName);

			simlog_entry(msg_buf);
			lastEventLogged++;
			if (lastEventLogged >= EVENT_LIST_SIZE)
			{
				lastEventLogged = 0;
			}
		}
		while (lastCommentLogged != simmgr_shm->commentListNext)
		{
			if (strlen(simmgr_shm->commentList[lastCommentLogged].comment) == 0)
			{
				sprintf_s(msg_buf, BUF_SIZE,"Null Comment: lastCommentLogged is %d simmgr_shm->commentListNext is %d State is %d\n",
					lastCommentLogged, simmgr_shm->commentListNext, scenario_state);
				log_message("Error", msg_buf);
			}
			else
			{
				simlog_entry(simmgr_shm->commentList[lastCommentLogged].comment);
			}
			lastCommentLogged++;
			if (lastCommentLogged >= COMMENT_LIST_SIZE)
			{
				lastCommentLogged = 0;
			}
		}
		releaseInstructorLock();
	}
}

void
strToLower(char* buf)
{
	int i;
	for (i = 0; buf[i] != 0; i++)
	{
		buf[i] = (char)tolower(buf[i]);
	}
}


void
simmgrRun(void)
{
	scenarioCount++;
	switch (scenarioCount)
	{
	case SCENARIO_COMMCHECK:
		comm_check();
		break;
	case SCENARIO_EVENTCHECK:
		checkEvents();
		break;
	case SCENARIO_CPRCHECK:
		cpr_check();
		break;
	case SCENARIO_SHOCKCHECK:
		shock_check();
		break;
	case SCENARIO_AWRRCHECK:
		awrr_check();
		break;
	case SCENARIO_TIMECHECK:
		time_update();
		break;
	case SCENARIO_LOOP_COUNT:
		scenarioCount = 0;
		(void)scan_commands();
		break;
	default:
		break;
	}
}

int
updateScenarioState(ScenarioState new_state)
{
	int rval = true;
	ScenarioState old_state;
	old_state = scenario_state;
	if (new_state != old_state)
	{
		if ((new_state == ScenarioState::ScenarioTerminate) && ((old_state != ScenarioState::ScenarioRunning) && (old_state != ScenarioState::ScenarioPaused)))
		{
			rval = false;
			sprintf_s(c_msgbuf, BUF_SIZE, "Scenario: Terminate requested while in state %d", old_state);
			simlog_entry(c_msgbuf);
		}
		else if ((new_state == ScenarioState::ScenarioPaused) && ((old_state != ScenarioState::ScenarioRunning) && (old_state != ScenarioState::ScenarioPaused)))
		{
			rval = false;
			sprintf_s(c_msgbuf, BUF_SIZE, "Scenario: Pause requested while in state %d", old_state);
			simlog_entry(c_msgbuf);
		}
		else
		{
			scenario_state = new_state;

			switch (scenario_state)
			{
			case ScenarioState::ScenarioStopped: // Set by scenario manager after Terminate Cleanup is complete
				sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "Stopped");
				if (simmgr_shm->status.scenario.record > 0)
				{
					recordStartStop(0);
				}
				(void)simlog_end();
				(void)resetAllParameters();
				break;
			case ScenarioState::ScenarioRunning:
				if (old_state == ScenarioState::ScenarioPaused)
				{
					sprintf_s(c_msgbuf, BUF_SIZE, "Scenario: Resume");
					simlog_entry(c_msgbuf);
				}
				sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "Running");
				break;
			case ScenarioState::ScenarioPaused:
				sprintf_s(c_msgbuf, STR_SIZE, "Scenario: Pause");
				simlog_entry(c_msgbuf);
				sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "Paused");
				break;
			case ScenarioState::ScenarioTerminate:	// Request from SIM II
				sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "Terminate");
				break;
			default:
				sprintf_s(simmgr_shm->status.scenario.state, STR_SIZE, "Unknown");
				break;
			}
			sprintf_s(c_msgbuf, STR_SIZE, "State: %s ", simmgr_shm->status.scenario.state);
			log_message("", c_msgbuf);
		}
	}
	return (rval);
}