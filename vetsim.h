#pragma once

/*
 * vetsim.h
 *
 * Simulation Manager
 *
 * This file is part of the sim-mgr distribution
 *
 * Copyright (c) 2021 VetSim, Cornell University College of Veterinary Medicine Ithaca, NY
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
#include <WinSDKVer.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") 

#include <iostream>
#include <winbase.h>
#include <direct.h>
#include <cstdio>
#include <conio.h>
#include <tchar.h>
#include <strsafe.h>
#include <Lmcons.h>
#include <time.h>
#include <synchapi.h>
#include <string.h>
#include <csignal>

#include <tchar.h>

#include <chrono>
#include <thread>
#include <functional>
#include <mutex>
#include <vector>
#include <locale>
#include <sstream>

#include <sal.h>
#include "vetsimTasks.h"
#include "version.h"

// Defines
//

#define STR_SIZE			64
#define LONG_STR_SIZE		128
#define FILENAME_SIZE		256
#define COMMENT_SIZE		1024
#define LOG_TO_FILE			0
#define MSG_LENGTH	2048

// Port numbers should be configurable
#define DEFAULT_PORT_PULSE			40844	// Note: If changed, must also change in SimController
#define DEFAULT_PORT_STATUS			40845	// Note: If changes, must also change in Sim-II
#define DEFAULT_PHP_SERVER_PORT		8081
#define DEFAULT_PHP_SERVER_ADDRESS	"127.0.0.1"
#define DEFAULT_LOG_NAME			"simlogs/vetsim.log"
#define DEFAULT_HTML_PATH			"html"

struct localConfiguration
{
	int port_pulse;
	int port_status;
	int php_server_port;
	char php_server_addr[STR_SIZE];
	char log_name[FILENAME_SIZE];
	char html_path[FILENAME_SIZE];
};


extern struct localConfiguration localConfig;

#define PORT_PULSE			(localConfig.port_pulse)
#define PORT_STATUS			(localConfig.port_status)
#define PHP_SERVER_PORT		(localConfig.php_server_port)
#define PHP_SERVER_ADDR		localConfig.php_server_addr
#define LOG_NAME			localConfig.log_name
#define HTML_PATH			localConfig.html_path

using namespace std;



// Terminate a running scenario after the limit is reached
#define MAX_SCENARIO_RUNTIME (1*60*60)	// 1 Hour, time in seconds

// When initSHM is called, only the simmgr daemon should set OPEN_WITH_CREATE
// All other open with _OPEN_ACCESS
#define	OPEN_WITH_CREATE		1
#define OPEN_ACCESS				0


enum class ScenarioState
{
	ScenarioStopped = 0,
	ScenarioRunning = 1,
	ScenarioPaused = 2,
	ScenarioTerminate = 3
};

enum class NibpState
{
	NibpIdle = 0,
	NibpWaiting = 1,
	NibpRunning = 2
};
#define NIBP_RUN_TIME	(15)	// Delay time in seconds


// Data Structures
//
struct cardiac
{
	char rhythm[STR_SIZE];
	char vpc[STR_SIZE];		// Format is T-C where T is type (1 or 2) and C is count (1, 2 or 3). Or "none" for no VPCs
	int vpc_freq;		// 0-100% - Frequency of VPC insertions (when vpc is not set to "none")
	int vpc_delay;		// Unused
	int vpc_count;		// Parsed from vpc
	int vpc_type;		// Parsed from vpc
	char vfib_amplitude[STR_SIZE];	// low, med, high
	int pea;			// Pulse-less Electrical Activity
	int rate;			// Heart Rate in Beats per Minute
	long int avg_rate;		// Calculated heart rate
	int nibp_rate;		// Non-Invasive Rate - Only reports when cuff is on
	int nibp_read;		// Set to 1 to start reading, set to 0 when reading is complete.
	int nibp_linked_hr;		// Set to 1 to keep NIBP linked with Cardiac Rate, set to 0 to unlink.
	int nibp_freq;		// Number of minutes for NIBP timer. 0 is manual.
	int transfer_time;	// Trend length for change in rate;
	char pwave[STR_SIZE];
	long int pr_interval;	// PR interval in msec
	long int qrs_interval;		// QRS in msec
	int bps_sys;	// Systolic
	int bps_dia;	// Diastolic
	int right_dorsal_pulse_strength; 	// 0 - None, 3 - strong
	int right_femoral_pulse_strength;
	int left_dorsal_pulse_strength;
	int left_femoral_pulse_strength;
	unsigned int pulseCount;
	unsigned int pulseCountVpc;

	char heart_sound[STR_SIZE];
	int heart_sound_volume;
	int heart_sound_mute;
	int ecg_indicator;
	int bp_cuff;
	int arrest;
};

struct scenario
{
	char active[STR_SIZE];		// Name of active scenario
	char start[STR_SIZE];		// Date/Time scenario started
	char runtimeAbsolute[STR_SIZE];
	char runtimeScenario[STR_SIZE];
	char runtimeScene[STR_SIZE];
	char clockDisplay[STR_SIZE];
	char state[STR_SIZE];
	char scene_name[LONG_STR_SIZE];	// Currently running scene
	char error_message[LONG_STR_SIZE]; // From scenario process, on failure
	int error_flag;				  // Set to indicate error
	int scene_id;				// Currently running scene
	int record;					// Set in initiator section to start/stop video recording
	time_t startTime;
	struct tm	tmStart;
	ULONGLONG elapsed_msec_scenario;
	ULONGLONG elapsed_msec_scene;
};

struct respiration
{
	// Sounds for Inhalation, Exhalation and Background
	char left_lung_sound[STR_SIZE];		// Base Sound 
	char left_sound_in[STR_SIZE];
	char left_sound_out[STR_SIZE];
	char left_sound_back[STR_SIZE];
	int left_lung_sound_volume;
	int left_lung_sound_mute;

	char right_lung_sound[STR_SIZE];		// Base Sound
	char right_sound_in[STR_SIZE];
	char right_sound_out[STR_SIZE];
	char right_sound_back[STR_SIZE];
	int right_lung_sound_volume;
	int right_lung_sound_mute;

	//char left_lung_sound[STR_SIZE];
	//char right_lung_sound[STR_SIZE];

	// Duration of in/ex
	int inhalation_duration;	// in msec
	int exhalation_duration;	// in msec

	int spo2;					// 0-100%
	int rate;					// Breaths per minute
	int awRR;					// Calculated rate
	int etco2;					// End Tidal CO2
	int transfer_time;			// Trend length for change in rate;
	int etco2_indicator;
	int spo2_indicator;
	int chest_movement;
	int manual_breath;			// Set to inject manual breath.		
	unsigned int manual_count;			// Total of Manual Breaths injected (From II or detected bagging)

	unsigned int breathCount;
};

struct auscultation
{
	int side;	// 0 - None, 1 - L:eft, 2 - Right
	int row;	// Row 0 is closest to spine
	int col;	// Col 0 is closets to head
};

#define PULSE_NOT_ACTIVE					0
#define PULSE_RIGHT_DORSAL					1
#define PULSE_RIGHT_FEMORAL					2
#define PULSE_LEFT_DORSAL					3
#define PULSE_LEFT_FEMORAL					4

#define PULSE_TOUCH_NONE					0
#define PULSE_TOUCH_LIGHT					1
#define PULSE_TOUCH_NORMAL					2
#define PULSE_TOUCH_HEAVY					3
#define PULSE_TOUCH_EXCESSIVE				4

struct pulse
{
	int right_dorsal;	// Touch Pressure
	int left_dorsal;	// Touch Pressure
	int right_femoral;	// Touch Pressure
	int left_femoral;	// Touch Pressure
};

struct cpr
{
	ULONGLONG last;			// msec time of last compression
	int	compression;	// 0 to 100%
	int release;		// 0 to 100%
	int duration;
	int running;
};
struct defibrillation
{
	ULONGLONG last;			// msec time of last shock
	int energy;			// Energy in Joules of shock
	int shock;			// Request a shock event
};

struct hdr
{
	char version[STR_SIZE];
	int size;
};

struct server
{
	char name[STR_SIZE]; 		// SimCtl Hostname
	char ip_addr[STR_SIZE];		// ETH0 Network IP Address
	char wifi_ip_addr[STR_SIZE];		// WiFi Network IP Address
	char server_time[STR_SIZE];	// Linux date/timestamp
	int pulsePort;					// IP Port number for Pulse Listen
	int statusPort;					// IP port number for simstatus
	ULONGLONG msec_time;				// msec timer.
	int dbg1;
	int dbg2;
	int dbg3;

};

struct general
{
	int temperature;			// degrees * 10, (eg 96.8 is 968)
	int transfer_time;			// Trend length
	int temperature_enable;		// 0 : No Probe, 1 : Probe Attached
	char temperature_units[4];	// F or C are valid
	char clockStart[STR_SIZE];	// Used to set the display time start
	int clockStartSec;
};

struct media
{
	char filename[FILENAME_SIZE];
	int play;
};

#define TSIM_WINDOWS	2

#define TSV_CMD_STOP	0
#define TSV_CMD_START	1
#define TSV_CMD_SEEK	2	// param is Time 

struct telesimVideo
{
	char name[STR_SIZE];	// Name specifies the element within the scenario
	int command;
	double param;
	int next;	// Incremented when a command is passed
};
struct telesim
{
	int enable;	// 0 off, 1 on
	struct telesimVideo	vid[TSIM_WINDOWS];
};

struct vocals
{
	char filename[FILENAME_SIZE];
	int repeat;
	int volume;
	int play;
	int mute;
};
struct logfile
{
	HANDLE 	sema;	// Mutex lock - Used to allow multiple writers
	int		active;
	int		lines_written;
	char	filename[FILENAME_SIZE];
	char	vfilename[FILENAME_SIZE];
};
struct status
{
	// Status of controlled parameters
	struct cardiac			cardiac;
	struct scenario 		scenario;
	struct respiration		respiration;
	struct general			general;
	struct vocals			vocals;
	struct media			media;

	// Status of sensed actions
	struct auscultation		auscultation;
	struct pulse			pulse;
	struct cpr				cpr;
	struct defibrillation	defibrillation;
	struct telesim			telesim;
	char 	eventName[STR_SIZE];
};

// The instructor structure is commands from the Instructor Interface
struct instructor
{
	HANDLE sema;	// Mutex lock
	struct cardiac		cardiac;
	struct scenario 	scenario;
	struct respiration	respiration;
	struct general		general;
	struct vocals		vocals;
	struct media		media;
	struct cpr			cpr;
	struct defibrillation	defibrillation;
	struct telesim			telesim;
	char	eventName[STR_SIZE];
};

struct event_inj
{
	time_t	time;
	char 	eventName[STR_SIZE];
};

struct comment_inj
{
	char 	comment[COMMENT_SIZE];
};
#define EVENT_LIST_SIZE			128
#define COMMENT_LIST_SIZE		16
#define RESP_HISTORY_DEPTH		5
#define CARDIAC_HISTORY_DEPTH	10
#define DECAY_SECONDS			10
#define MS_PER_MIN				(60*1000)


// List of attached Listeners SimControllers
#define MAX_CONTROLLERS			20
struct simControllers
{
	int allocated;
	char ipAddr[256];
};
// Data Structure of Shared memory file
struct simmgr_shm
{
	// Static data describing the data simmgr_shm structure for verification.
	struct hdr hdr;

	// Dynamic data relating to the Server System
	struct server server;

	// SimMgr Status - Written by the SimMgr only. Read from all others
	struct status status;

	// Commands from Instructor Interface. Written by the II Ajax calls. Cleared by SimMgr when processed.
	struct instructor instructor;

	// Log file status
	struct logfile logfile;

	int eventListBuffer;

	// Event List - Used to post multiple messages to the various listeners
	// Must only be written when instructor.sema is held
	int eventListNext;	// Index to the last event written ( 0 to EVENT_LIST_SIZE-1 )
	struct event_inj	eventList[EVENT_LIST_SIZE];

	// Comment List - Used to post comments into the log
	// Must only be written when instructor.sema is held
	int commentListNext;	// Index to the last comment written ( 0 to COMMENT_LIST_SIZE-1 )
	struct comment_inj	commentList[COMMENT_LIST_SIZE];

	int respTimeList[RESP_HISTORY_DEPTH];	// ms per beat
	int respTimeNextWrite;

	int cardiacTimeList[CARDIAC_HISTORY_DEPTH];	// ms per beat
	int cardiacTimeNextWrite;

	struct simControllers simControllers[MAX_CONTROLLERS];
};

// For generic trend processor
struct trend
{
	double start;
	double end;
	double current;
	double changePerSecond;
	time_t nextTime;
	time_t endTime;
	int seconds;
};

// Prototypes
// 
int	initSHM(int create, char* sesid);
void log_message(const char* filename, const char* message);
void log_messaget(const char* filename, TCHAR* message);
char* do_command_read(const char* cmd_str, char* buffer, int max_len);
void get_date(char* buffer, int maxLen);
char* getETH0_IP();
char* getWIFI_IP();
#ifndef WIN32
char* nth_occurrence(char* haystack, char* needle, int nth);
void daemonize(void);
int kbhit(void);
int checkExit(void);
char* _itoa(int val, char* buf, int radix);
char* _ltoa(long int val, char* buf, int radix);
void signal_fault_handler(int sig);
#else
void showLastError(LPTSTR lpszFunction);
#endif
void cleanString(char* strIn);

// Defines and protos for sim-log
#define SIMLOG_MODE_READ	0
#define SIMLOG_MODE_WRITE	1
#define SIMLOG_MODE_CREATE	2

int simlog_create(void);			// Create new file
int simlog_open(int rw);			// Open for Read or Write - Takes Mutex if writing
int simlog_write(char* msg);		// Write line
size_t simlog_read(char* rbuf);		// Read next line
size_t simlog_read_line(char* rbuf, int lineno);		// Read line from line number
void simlog_close();				// Closes file and release Mutex if held
void simlog_end();
void simlog_entry(char* msg);
int takeInstructorLock();
void releaseInstructorLock();
void addEvent(char* str);
void addComment(char* str);
void lockAndComment(char* str);
void forceInstructorLock(void);
void awrr_restart(void);
ULONGLONG msec_time_update(void);
std::string GetLastErrorAsString(void);

// Shared Parse functions
int cardiac_parse(const char* elem, const char* value, struct cardiac* card);
int respiration_parse(const char* elem, const char* value, struct respiration* resp);
int general_parse(const char* elem, const char* value, struct general* gen);
int telesim_parse(const char* elem, const char* value, struct telesim* ts);
int vocals_parse(const char* elem, const char* value, struct vocals* voc);
int media_parse(const char* elem, const char* value, struct media* med);
int cpr_parse(const char* elem, const char* value, struct cpr* cpr);
void initializeParameterStruct(struct instructor* initParams);
void processInit(struct instructor* initParams);
int getValueFromName(char* param_class, char* param_element);
// Global Data
//
#ifndef SIMUTIL
extern int simmgrSHM;					// The File for shared memory
extern struct simmgr_shm* simmgr_shm;	// Data structure of the shared memory
#endif

// Used for deamonize only
extern char* program_invocation_name;
extern char* program_invocation_short_name;

void pulseProcessChild(void);
int pulseTask(void);

int scenario_main(void);

int clock_gettime(int X, struct timeval* tv);
#define CLOCK_REALTIME	1

int
getVideoFileCount(void);

#define MSG_BUFF_MAX	32

struct obsData
{
	HWND obsWnd;
	DWORD obsPid;
	char newFilename[512];
};

#define	OPEN_WITH_CREATE		1
#define OPEN_ACCESS				0

int vetsim(void);

//In simmgrCommon
void simmgrRun(void);
int scan_commands(void);
void comm_check(void);
void time_update(void);
void awrr_check(void);
void cpr_check(void);
void shock_check(void);
int start_scenario(void);
void checkEvents(void);
void clearAllTrends(void);
void resetAllParameters(void);
void setRespirationPeriods(int oldRate, int newRate);
void strToLower(char* buf);

// In simmgrVideo
int recordStartStop(int record);
int getVideoFileCount(void);
void closeVideoCapture(void);