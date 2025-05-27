/*
 * scenario.cpp
 *
 * Scenario Processing
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSim/sim-mgr).
 *
 * Copyright (c) 2019-2025 ITown Design, Cornell University College of Veterinary Medicine Ithaca, NY
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
 * The Scenario runs as an independent process. It is exec'ed from the simmgr and acts
 * using the shared memory space to monitor the system and inject Initiator commands
 * as controlled by the scenario script.
 *
 * The scenario script is an XML formatted file. It is parsed by the scenario process to
 * define the various scenes. A scene contains a set of initialization parameters and
 * a set a triggers.
 *
 * The scenario process runs as a child process of the simmgr daemon. It is started by
 * the simmgr on command from the Instructor Interface and can be terminated on command
 * as well.
 *
 * On completion of the scenario, the scenario process is expected to enter a "Ending"
 * scene. More than one ending scene may exist, allow outcomes with either a healthy
 * or expired patient, or other states of health as well.
 *
 * Definitions:
 *
 * Init: A list of parameter definitions, with or without trends. The init parameters are
 *       applied at the entry to a scene.
 *       A global init is also defined. This is applied at the entry to the scenario.
 *
 *    Vocals in Init:
 * 		Vocals may be set in an init definition. This may be used to invoke a vocalization
 *      at the beginning of scene. The vocalization is invoked immediately when parsed.
 *
 * Scene: A state definition within the scenario.
 *
 * Trigger: A criteria for termination of a scene. The trigger defines the parameter to be
 *          watched, the threshold for firing and the next scene to enter.
 *
 * Trigger Group: A criteria for termination of a scene using a group of trigger conditions.
 *
 * Ending Scene: A scene that ends the scenario. This scene has an init, but no triggers.
 *          It also contains an <end> directive, which will cause the scenario process
 *          to end.
 *
 * Process End:
 *		On completion, the scenario process will print a single line and then exit. If the
 *      end is due to entry of an ending scene, the printed line is the content from the
 *		<end> directive. If exit is due to an error, the line will describe the error.
 */

#include "vetsim.h"
#include "scenario.h"
#include "llist.h"
// #include "XMLRead.h"

int current_scene_id = -1;
using namespace std;
const char* xml_filename;
int line_number = 0;
int verbose = 0;
int checkOnly = 0;
int errCount = 0;

extern int closeFlag;



//static void saveData(const xmlChar* xmlName, const xmlChar* xmlValue);
//static int readScenario(const char* filename);
static void scene_check(void);
static struct scenario_scene* findScene(int scene_id);

int validateScenes(void );
static void startScene(int sceneId);

// loopStart and loopStop are used to measure the actual sleep time of the scenario loop,
// to calculate the time in a scene and in the scenario
struct timeval loopStart;
struct timeval loopStop;

// palpateStart and palpateStop are used to measure the duration of palpation,
struct timeval palpateStart;
struct timeval palpateNow;

//int eventLast;	// Index of last processed event_callback

struct timeval cprStart; // Time of first CPR detected
int cprActive = 0;				// Flag to indicate CPR is active
int cprCumulative = 0;		// Cumulative time for CPR active in this scene
int shockActive = 0;	// Flag to indicate Defibrillation is active
struct pulse pulseStatus = { 0, 0, 0, 0, 0, 0 };

// Internal state is tracked to compare to the overall state, for detecting changes
ScenarioState proc_scenario_state;

char logMsg[512];

#include <windows.h>

#ifdef SCENARIO_WINDOW_OUT
// Global variables
extern HINSTANCE hInst;
HWND hWnd;
LPCWSTR szWindowClass = L"ScenarioWindowClass";
LPCWSTR szTitle = L"Scenario Text Display";


// Forward declarations of functions included in this code module
extern CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Function to register the window class
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	return RegisterClassExW(&wcex);
}

// Function to initialize and display the window
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, LPCWSTR text)
{
	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) {
		return FALSE;
	}

	// Set the text to be displayed in the window
	SetWindowTextW(hWnd, text);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

// Window procedure function to handle messages
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// Add any drawing code here...
		EndPaint(hWnd, &ps);
	}
				 break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Function to display a new window with text
void showTextWindow(HINSTANCE hInstance, int nCmdShow, LPCWSTR text) {
	MyRegisterClass(hInstance);
	if (!InitInstance(hInstance, nCmdShow, text)) {
		MessageBoxW(nullptr, L"Call to InitInstance failed!", szTitle, NULL);
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void displayParseLog(void)
{
    // Convert std::wstring to LPCWSTR
    //showTextWindow(GetModuleHandle(NULL), SW_SHOW, L"Parse Errors");

    // Allocate memory for the text string
    size_t textLength = (parseLog.length() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, textLength);
    if (hGlobal == nullptr) {
        return; // Failed to allocate memory
    }
    // Copy the text to the allocated memory
    LPVOID pGlobal = GlobalLock(hGlobal);
    if (pGlobal == nullptr) {
        GlobalFree(hGlobal);
        return; // Failed to lock memory
    }
	memcpy(hGlobal, parseLog.c_str(), textLength);
	GlobalUnlock(hGlobal);

    // Prepare COPYDATASTRUCT
    COPYDATASTRUCT copyDataStruct;
    copyDataStruct.dwData = 0;
    copyDataStruct.cbData = static_cast<DWORD>(textLength);
    copyDataStruct.lpData = hGlobal;

	

    // Post the message with the text string as the message data
    //BOOL result = PostMessage(hWnd, WM_COPYDATA, (WPARAM)nullptr, (LPARAM)&copyDataStruct);

    // Clean up allocated memory
    GlobalFree(hGlobal);
}
#endif

#define MAX_MSG_SIZE 1024
char s_msg[MAX_MSG_SIZE];

int xml_current_level = 0;
std::wstring parseLog;
int parse_state = PARSE_STATE_NONE;
int parse_init_state = PARSE_INIT_STATE_NONE;
int parse_scene_state = PARSE_SCENE_STATE_NONE;
int parse_header_state = PARSE_HEADER_STATE_NONE;

struct scenario_data* scenario;
struct scenario_scene* current_scene;

int
scenario_main(void)
{
	int sts;
	char* sesid = NULL;
	struct tm tmDest;
	time_t start_time;
	errno_t err = 0;

	snprintf(s_msg, MAX_MSG_SIZE, "Scenario File \"%s\"", simmgr_shm->status.scenario.active);
	if (!checkOnly)
	{
		log_message("", s_msg);
	}
	if (verbose)
	{
		fprintf(stderr, "%s\n", s_msg);
	}

	xml_current_level = 0;
	current_scene_id = -1;
	line_number = 0;

	parse_state = PARSE_STATE_NONE;
	parse_init_state = PARSE_INIT_STATE_NONE;
	parse_scene_state = PARSE_SCENE_STATE_NONE;
	parse_header_state = PARSE_HEADER_STATE_NONE;

	cprActive = 0;				// Flag to indicate CPR is active
	cprCumulative = 0;		// Cumulative time for CPR active in this scene
	shockActive = 0;	// Flag to indicate Defibrillation is active
	pulseStatus.left_femoral = false;
	pulseStatus.right_femoral = false;
	pulseStatus.left_dorsal = false;
	pulseStatus.right_dorsal = false;
	pulseStatus.active = false;
	pulseStatus.duration = 0;

	simmgr_shm->status.scenario.elapsed_msec_scenario = 0;
	simmgr_shm->status.scenario.elapsed_msec_scene = 0;

	// Allocate and clear the base scenario structure
	scenario = (struct scenario_data*)calloc(1, sizeof(struct scenario_data));
	initializeParameterStruct(&scenario->initParams);

	simmgr_shm->eventListNextWrite = 0;	// Start processing event at the next posted event
	simmgr_shm->eventListNextRead = 0;

	// For display Time
	start_time = std::time(nullptr);
	err = localtime_s(&tmDest, &start_time);
	simmgr_shm->status.general.clockStartSec = (tmDest.tm_hour * 60 * 60) + (tmDest.tm_min * 60) + tmDest.tm_sec;

	// Initialize the library and check potential ABI mismatches 

	if (readScenario(simmgr_shm->status.scenario.active) < 0)
	{
		printf("readScenario Fails\n");
		snprintf(s_msg, MAX_MSG_SIZE, "scenario: readScenario Fails");
		if (!checkOnly)
		{
			log_message("", s_msg);
		}
		if (verbose)
		{
			fprintf(stderr, "%s\n", s_msg);
		}
		takeInstructorLock();
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "stopped");
		simmgr_shm->instructor.scenario.error_flag = 1;
		releaseInstructorLock();
		return (-1);
	}
	if (errCount)
	{

	}
	printf("readScenario Success\n");
	if (verbose || checkOnly)
	{
		printf("Showing scenes\n");
		showScenes();
	}
	if (verbose)
	{
		printf("Calling findScene for scenario\n");
	}
	snprintf(s_msg, MAX_MSG_SIZE, "scenario: Calling findScene for scenario");
	if (!checkOnly)
	{
		log_message("", s_msg);
	}
	if (verbose)
	{
		fprintf(stderr, "%s\n", s_msg);
	}

	// Get the name of the current scene
	current_scene = findScene(current_scene_id);
	if (!current_scene)
	{
		snprintf(s_msg, MAX_MSG_SIZE, "Scenario: Starting scene not found in XML file");
		printf("Scenario: Starting scene not found in XML file\n");
		current_scene_id = -1;
		if (!checkOnly)
		{
			lockAndComment(s_msg);
		}
		if (verbose)
		{
			fprintf(stderr, "%s\n", s_msg);
		}
		if (!checkOnly)
		{
			printf("No Start Scene\n");
			sprintf_s(simmgr_shm->status.scenario.scene_name, STR_SIZE, "%s", "No Start Scene");
			takeInstructorLock();
			sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "terminate");
			sprintf_s(simmgr_shm->instructor.scenario.error_message, STR_SIZE, "%s", "No Start Scene");
			simmgr_shm->instructor.scenario.error_flag = 1;
			releaseInstructorLock();
		}
		parseLog.append(L"Starting scene not found in XML file\n"); 
		errCount++;
	}
	else
	{
		if (!checkOnly)
		{
			sprintf_s(simmgr_shm->status.scenario.scene_name, STR_SIZE, "%s", current_scene->name);
		}
	}

	if ( errCount )
	{
		sprintf_s(simmgr_shm->status.scenario.scene_name, STR_SIZE, "%s", "Errors in XML file. See the log for details.");
		takeInstructorLock();
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "terminate");
		sprintf_s(simmgr_shm->instructor.scenario.error_message, STR_SIZE, "%s", "Errors in XML file. See the log for details.");
		simmgr_shm->instructor.scenario.error_flag = 1;
		releaseInstructorLock();
		printf("erCount is %d\n", errCount);
		//displayParseLog();
	}
	else if (checkOnly)
	{
		sprintf_s(simmgr_shm->status.scenario.scene_name, STR_SIZE, "%s errCount is %d", "Check Only", errCount);
		takeInstructorLock();
		sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", "terminate");
		sprintf_s(simmgr_shm->instructor.scenario.error_message, STR_SIZE, "%s errCount is %d", "Check Only", errCount );
		simmgr_shm->instructor.scenario.error_flag = 1;
		printf("checkOnly is %d\n", checkOnly);
		releaseInstructorLock(); 
	}

	if (verbose)
	{
		printf("\n\n\nCalling processInit for scenario\n");
	}
	///snprintf(s_msg, MAX_MSG_SIZE, "scenario: Calling processInit for scenario" );
	//log_message("", s_msg );
	simmgr_shm->status.cpr.compression = 0;
	simmgr_shm->status.cpr.duration = 0;
	simmgr_shm->status.defibrillation.energy = 100;
	simmgr_shm->status.defibrillation.shock = 0;
	simmgr_shm->status.cardiac.bp_cuff = 0;
	simmgr_shm->status.cardiac.ecg_indicator = 0;
	simmgr_shm->status.cardiac.pea = 0;
	simmgr_shm->status.cardiac.arrest = 0;
	simmgr_shm->status.respiration.etco2_indicator = 0;
	simmgr_shm->status.respiration.spo2_indicator = 0;
	simmgr_shm->status.respiration.chest_movement = 0;
	simmgr_shm->status.respiration.manual_breath = 0;
	simmgr_shm->status.respiration.manual_count = 0;
	simmgr_shm->status.general.temperature_enable = 0;


	// Log the Scenario Name
	snprintf(s_msg, MAX_MSG_SIZE, "Title: %s", scenario->title);
	lockAndComment(s_msg);
	
	//snprintf(s_msg, MAX_MSG_SIZE, "Author: %s", scenario->author );
	//lockAndComment(s_msg );
	//snprintf(s_msg, MAX_MSG_SIZE, "Created: %s", scenario->date_created );
	//lockAndComment(s_msg );
	//snprintf(s_msg, MAX_MSG_SIZE, "Description: %s", scenario->description );
	//lockAndComment(s_msg );
	

	// Apply initialization parameters
	processInit(&scenario->initParams);

	if (current_scene_id >= 0)
	{
		if (verbose)
		{
			printf("Calling processInit for Scene %d, %s \n", current_scene->id, current_scene->name);
		}
		startScene(current_scene_id);
	}

	// Set our internal state to running
	proc_scenario_state = ScenarioState::ScenarioRunning;
	
	if (verbose)
	{
		printf("Starting Loop\n");
	}

	simmgr_shm->status.scenario.elapsed_msec_scenario = 0;
	simmgr_shm->status.scenario.elapsed_msec_scene = 0;

	extern std::time_t scenario_start_time;
	scenario_start_time = time(nullptr);

	cprActive = 0;
	cprCumulative = 0;
	shockActive = 0;

	//if (validateScenes() != 0)
	//{
		//errno = -1;
	//}
	// Continue scenario execution
	while (1)
	{
		clock_gettime(CLOCK_REALTIME, &loopStart);

		// Sleep
		Sleep(SCENARIO_LOOP_DELAY);
		if (simmgr_shm->status.defibrillation.shock == 1)
		{
			continue;
		}
		if (strcmp(simmgr_shm->status.scenario.state, "Terminate") == 0)	// Check for termination
		{
			if (proc_scenario_state != ScenarioState::ScenarioTerminate)
			{
				// If the scenario needs to do any cleanup, this is the place.
				printf("Scenario is Terminating\n");
				snprintf(s_msg, MAX_MSG_SIZE, "Scenario: Terminate");
				//log_message("", s_msg );

				sts = takeInstructorLock();
				if (!sts)
				{
					addComment(s_msg);
					proc_scenario_state = ScenarioState::ScenarioTerminate;
					sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "Stopped");
					releaseInstructorLock();
					printf("Scenario is Stopping\n");
				}
				else
				{
					printf("Failed to get the Instructor Lock\n");
				}
			}
		}
		else if (strcmp(simmgr_shm->status.scenario.state, "Stopped") == 0)
		{
			if (proc_scenario_state != ScenarioState::ScenarioStopped)
			{
				snprintf(s_msg, MAX_MSG_SIZE, "Scenario: Stopped");
				//log_message("", s_msg );
				lockAndComment(s_msg);
				proc_scenario_state = ScenarioState::ScenarioStopped;
				printf("Scenario process is exiting\n");
				free(scenario);
				return(0);
			}
		}
		else if (strcmp(simmgr_shm->status.scenario.state, "Running") == 0)
		{
			// Do periodic scenario check
			scene_check();
			proc_scenario_state = ScenarioState::ScenarioRunning;
		}
		else if (strcmp(simmgr_shm->status.scenario.state, "Paused") == 0)
		{
			// Nothing
			proc_scenario_state = ScenarioState::ScenarioPaused;
		}
		if (closeFlag)
		{
			break;
		}
	}

	printf("Exit scenario_main");
	return(0);
}

/**
 * findScene
 * @scene_id
 *
*/
static struct scenario_scene*
findScene(int scene_id)
{
	struct snode* snode;
	struct scenario_scene* scene;
	//int limit = 50;

	snode = scenario->scene_list.next;

	while (snode)
	{
		scene = (struct scenario_scene*)snode;
		if (scene->id == scene_id)
		{
			return (scene);
		}
		/*
		if ( limit-- == 0 )
		{
			printf("findScene Limit reached\n" );
			sprintf(s_msg, "Scenario: findScene Limit reached" );
			log_message("", s_msg );
			lockAndComment(s_msg );
			exit ( -2 );
		}
		*/
		snode = get_next_llist(snode);
	}
	return (NULL);
}


void
logTriggerGroup(struct trigger_group* trig_group, int time)
{
	snprintf(s_msg, MAX_MSG_SIZE, "Group Trigger:" );

	lockAndComment(s_msg);
}
extern const char* trigger_tests_sym[];

void
logTrigger(struct scenario_trigger* trig, int time)
{
	if (trig)
	{
		switch (trig->test)
		{
		case TRIGGER_TEST_EVENT:
			snprintf(s_msg, MAX_MSG_SIZE, "Trigger: Event %s", trig->param_element);
			break;

		case TRIGGER_TEST_INSIDE:
			snprintf(s_msg, MAX_MSG_SIZE, "Trigger: %d < %s:%s < %d",
				trig->value, trig->param_class, trig->param_element, trig->value2);
			break;

		case TRIGGER_TEST_OUTSIDE:
			snprintf(s_msg, MAX_MSG_SIZE, "Trigger: %d > %s:%s > %d",
				trig->value, trig->param_class, trig->param_element, trig->value2);
			break;

		default:
			snprintf(s_msg, MAX_MSG_SIZE, "Trigger: %s:%s %s %d",
				trig->param_class, trig->param_element, trigger_tests_sym[trig->test], trig->value);
			break;
		}
	}
	else if (time)
	{
		snprintf(s_msg, MAX_MSG_SIZE, "Trigger: Timeout %d seconds", time);
	}
	else
	{
		snprintf(s_msg, MAX_MSG_SIZE, "Trigger: unknown");
	}

	lockAndComment(s_msg);
}

/**
* pulse_check
*
* Check for start/stop palpations
*/


static void pulse_check(void)
{
	if (!pulseStatus.right_dorsal && simmgr_shm->status.pulse.right_dorsal)
	{
		pulseStatus.right_dorsal = true;
		pulseStatus.active = 1;
		clock_gettime(CLOCK_REALTIME, &palpateStart);
		snprintf(s_msg, MAX_MSG_SIZE, "Action: Start Pulse Palpation Right Dorsal ");
		lockAndComment(s_msg);
	}
	else if (pulseStatus.right_dorsal && !simmgr_shm->status.pulse.right_dorsal)
	{
		pulseStatus.right_dorsal = false;
		pulseStatus.active = 0;
		simmgr_shm->status.pulse.duration = 0;
		snprintf(s_msg, MAX_MSG_SIZE, "Action: End Pulse Palpation Right Dorsal ");
		lockAndComment(s_msg);
	}

	if (!pulseStatus.left_dorsal && simmgr_shm->status.pulse.left_dorsal)
	{
		pulseStatus.left_dorsal = true;
		pulseStatus.active = 1;
		clock_gettime(CLOCK_REALTIME, &palpateStart);
		snprintf(s_msg, MAX_MSG_SIZE, "Action: Start Pulse Palpation Left Dorsal ");
		lockAndComment(s_msg);
	}
	else if (pulseStatus.left_dorsal && !simmgr_shm->status.pulse.left_dorsal)
	{
		pulseStatus.left_dorsal = false;
		pulseStatus.active = 0;
		simmgr_shm->status.pulse.duration = 0;
		snprintf(s_msg, MAX_MSG_SIZE, "Action: End Pulse Palpation Left Dorsal ");
		lockAndComment(s_msg);
	}

	if (!pulseStatus.right_femoral && simmgr_shm->status.pulse.right_femoral)
	{
		pulseStatus.right_femoral = true;
		pulseStatus.active = 1;
		clock_gettime(CLOCK_REALTIME, &palpateStart);
		snprintf(s_msg, MAX_MSG_SIZE, "Action: Start Pulse Palpation Right Femoral ");
		lockAndComment(s_msg);
	}
	else if (pulseStatus.right_femoral && !simmgr_shm->status.pulse.right_femoral)
	{
		pulseStatus.right_femoral = false;
		pulseStatus.active = 0;
		simmgr_shm->status.pulse.duration = 0;
		snprintf(s_msg, MAX_MSG_SIZE, "Action: End Pulse Palpation Right Femoral ");
		lockAndComment(s_msg);
	}

	if (!pulseStatus.left_femoral && simmgr_shm->status.pulse.left_femoral)
	{
		pulseStatus.left_femoral = true;
		pulseStatus.active = 1;
		clock_gettime(CLOCK_REALTIME, &palpateStart);
		snprintf(s_msg, MAX_MSG_SIZE, "Action: Start Pulse Palpation Left Femoral ");
		lockAndComment(s_msg);
	}
	else if (pulseStatus.left_femoral && !simmgr_shm->status.pulse.left_femoral)
	{
		pulseStatus.left_femoral = false;
		pulseStatus.active = 0;
		simmgr_shm->status.pulse.duration = 0;
		snprintf(s_msg, MAX_MSG_SIZE, "Action: End Pulse Palpation Left Femoral ");
		lockAndComment(s_msg);
	}
	simmgr_shm->status.pulse.active = pulseStatus.active;
	if (pulseStatus.active)
	{
		int msec_diff;
		int sec_diff;
		clock_gettime(CLOCK_REALTIME, &palpateNow);
		sec_diff = (palpateNow.tv_sec - palpateStart.tv_sec);
		msec_diff = (((sec_diff * 1000000) + palpateNow.tv_usec) - palpateStart.tv_usec) / 1000;

		simmgr_shm->status.pulse.duration = msec_diff;
	}
}

/**
* triiger_check
* 
* Check for condition met for and event or trigger condition
*/
int trigger_check(struct scenario_trigger* trig )
{
	int met = 0;

	if (trig->test == TRIGGER_TEST_EVENT)
	{
		if (strcmp(trig->param_element, simmgr_shm->eventList[simmgr_shm->eventListNextRead].eventName) == 0)
		{
			met = 1;
			printf("Event %s MET!\n", trig->param_element);
		}
		//printf("Event %s %s %d %d %d %d Met %d\n",
		//	trig->param_class,
		//	trig->param_element,
		//	trig->test,
		//	trig->value,
		//	trig->value2,
		//	trig->scene, met );
	}
	else
	{
		int val;

		val = getValueFromName(trig->param_class, trig->param_element);
		switch (trig->test)
		{
		case TRIGGER_TEST_EQ:
			if (val == trig->value)
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_LTE:
			if (val <= trig->value)
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_LT:
			if (val < trig->value)
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_GTE:
			if (val >= trig->value)
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_GT:
			if (val > trig->value)
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_INSIDE:
			if ((val > trig->value) && (val < trig->value2))
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		case TRIGGER_TEST_OUTSIDE:
			if ((val < trig->value) || (val > trig->value2))
			{
				printf("Event %s MET!\n", trig->param_element);
				met = 1;
			}
			break;
		}
		//printf("Trig %s %s %d %d %d %d %d Met %d\n",
		//	trig->param_class, trig->param_element,
		//	trig->test,
		//	trig->value,
		//	trig->value2,
		//	trig->scene,
		//	trig->group,
		//	met );
	}
	return (met);
}
/**
* scene_check
*
* Scan through the events and triggers and change scene if needed
*/

static void
scene_check(void)
{
	struct scenario_trigger* trig;
	struct trigger_group* trig_group;
	struct snode* snode;
	struct snode* tsnode;
	int met = 0;
	int msec_diff;
	int sec_diff;

	// Event checks 
	while (simmgr_shm->eventListNextWrite != simmgr_shm->eventListNextRead )
	{
		snode = current_scene->trigger_list.next;
		
		// Event Checks - Single Triggers
		while (snode)
		{
			trig = (struct scenario_trigger*)snode;
			if (trig->test == TRIGGER_TEST_EVENT)
			{
				if ( trigger_check(trig ) )
				{
					logTrigger(trig, 0);
					startScene(trig->scene);
					return;
				}
			}
			snode = get_next_llist(snode);
		}
		// Event Checks - Group Triggers
		tsnode = current_scene->group_list.next;
		while (tsnode)
		{
			trig_group = (struct trigger_group*)tsnode;
			printf("Checking Group %d Event Trigs\n", trig_group->group_id );
			snode = trig_group->group_trigger_list.next;
			while (snode)
			{
				trig = (struct scenario_trigger*)snode;
				printf("\t\tTrigger %s %d %d\n", trig->param_element, trig->test, trig->met );
				if (!trig->met)
				{
					if (trig->test == TRIGGER_TEST_EVENT)
					{
						printf("Checking Group %d Trig Event %s\n", trig_group->group_id, trig->param_element);
						if (trigger_check(trig))
						{
							trig->met = 1;
							trig_group->group_triggers_met++;
							printf("Group %d Trig Event %s MET! Group Met is %d\n", trig_group->group_id, trig->param_element, trig_group->group_triggers_needed);
							if (trig_group->group_triggers_met >= trig_group->group_triggers_needed)
							{
								logTriggerGroup(trig_group, 0);
								startScene(trig_group->scene);
								return;
							}
						}
					}
				}
				snode = get_next_llist(snode);
			}
			tsnode = get_next_llist(tsnode);
		}
		printf("Processed Event %s\n", simmgr_shm->eventList[simmgr_shm->eventListNextRead].eventName);
		simmgr_shm->eventListNextRead++;
		if (simmgr_shm->eventListNextRead >= EVENT_LIST_SIZE)
		{
			simmgr_shm->eventListNextRead = 0;
		}
	}
	// Processed All Events on the list


	if (shockActive)
	{
	}
	if (cprActive)
	{
		if (simmgr_shm->status.cpr.compression == 0)
		{
			cprActive = 0;
			snprintf(s_msg, MAX_MSG_SIZE, "CPR: Stopping Compressions: Cumulative %d seconds", cprCumulative);
			lockAndComment(s_msg);
		}
		else
		{
			clock_gettime(CLOCK_REALTIME, &loopStop);
			cprCumulative += (loopStop.tv_sec - cprStart.tv_sec);
			clock_gettime(CLOCK_REALTIME, &cprStart);
			simmgr_shm->status.cpr.duration = cprCumulative;
		}
	}
	else
	{
		if (simmgr_shm->status.cpr.compression)
		{
			clock_gettime(CLOCK_REALTIME, &cprStart);
			cprActive = 1;
			snprintf(s_msg, MAX_MSG_SIZE, "CPR: Starting Compressions");
			lockAndComment(s_msg);
		}
	}

	// Pulse Palpation Checks
	pulse_check();

	// Trigger Checks - Single Triggers
	snode = current_scene->trigger_list.next;
	while (snode)
	{
		if (closeFlag)
		{
			break;
		}
		trig = (struct scenario_trigger*)snode;
		if (trig->test != TRIGGER_TEST_EVENT)
		{
			met = trigger_check(trig);

			if (met)
			{
				printf("Single Trigger Met\n");
				logTrigger(trig, 0);
				startScene(trig->scene);
				return;
			}
		}
		snode = get_next_llist(snode);
	}

	// Trigger Checks - Group Triggers
	snode = current_scene->group_list.next;
	while (snode)
	{
		if (closeFlag)
		{
			break;
		}
		trig_group = (struct trigger_group*)snode;
		tsnode = trig_group->group_trigger_list.next;
		while (tsnode)
		{
			if (closeFlag)
			{
				break;
			}
			trig = (struct scenario_trigger*)tsnode;
			if (!trig->met && trig->test != TRIGGER_TEST_EVENT)
			{
				met = trigger_check(trig);

				if (met)
				{
					trig->met = 1;
					trig_group->group_triggers_met++;
					printf("Group %d Trigger %s Gropup Met %d\n", trig_group->group_id, trig->param_element, trig_group->group_triggers_met );
					logTrigger(trig, 0);
					if (trig_group->group_triggers_met >= trig_group->group_triggers_needed)
					{
						logTriggerGroup(trig_group, 0);
						startScene(trig_group->scene);
						return;
					}
				}
			}
			tsnode = get_next_llist(tsnode);
		}
		snode = get_next_llist(snode);
	}

	// Check timeout
	clock_gettime(CLOCK_REALTIME, &loopStop);
	sec_diff = (loopStop.tv_sec - loopStart.tv_sec);
	//msec_diff = (((sec_diff * 1000000000) + loopStop.tv_nsec) - loopStart.tv_nsec) / 1000000;
	msec_diff = (((sec_diff * 1000000) + loopStop.tv_usec) - loopStart.tv_usec) / 1000;
	simmgr_shm->status.scenario.elapsed_msec_scenario += msec_diff;
	simmgr_shm->status.scenario.elapsed_msec_scene += msec_diff;

	if (current_scene->timeout)
	{
		if (simmgr_shm->status.scenario.elapsed_msec_scene >= ((ULONGLONG)current_scene->timeout * 1000))
		{
			logTrigger((struct scenario_trigger*)0, current_scene->timeout);
			startScene(current_scene->timeout_scene);
		}
	}
}

/** showSceen
*/
static void 
showScene(struct scenario_scene* scene)
{
	struct scenario_trigger* trig;
	struct trigger_group* trig_group;

	printf("Scene ID: %d\n", scene->id);

	printf("Triggers:\n");
	trig = (struct scenario_trigger* )scene->trigger_list.next;
	while (trig)
	{
		printf("Trig %s %s %d %d %d %d %d\n",
			trig->param_class, trig->param_element,
			trig->test,
			trig->value,
			trig->value2,
			trig->scene,
			trig->group);
		trig = (struct scenario_trigger*)trig->trigger_list.next;
	}
	printf("Trigger Groups:\n");
	trig_group = (struct trigger_group*)scene->group_list.next;
	while (trig_group)
	{
		printf("Needed: %d,  Next Scene %d \n", trig_group->group_triggers_needed, trig_group->scene);
		trig = (struct scenario_trigger*)trig_group->group_trigger_list.next;
		while (trig)
		{
			printf("    Trig %s %s %d %d %d %d %d\n",
				trig->param_class, trig->param_element,
				trig->test,
				trig->value,
				trig->value2,
				trig->scene,
				trig->group);
			trig = (struct scenario_trigger*)trig->trigger_list.next;
		}
		trig_group = (struct trigger_group*)trig_group->group_list.next;
	}
	
}
/** startScene
 * @sceneId: id of new scene
 *
*/
static void
startScene(int sceneId)
{
	struct scenario_scene* new_scene;

	new_scene = findScene(sceneId);
	if (!new_scene)
	{
		fprintf(stderr, "Scene %d not found", sceneId);
		printf("Scene %d not found", sceneId);
		snprintf(s_msg, MAX_MSG_SIZE, "Scenario: Scene %d not found. Terminating.", sceneId);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "Scenario: Scene %d not found. Terminating.", sceneId);
		takeInstructorLock();
		addComment(s_msg);
		sprintf_s(simmgr_shm->instructor.scenario.state, NORMAL_STRING_SIZE, "%s", "Terminate");
		sprintf_s(simmgr_shm->status.scenario.scene_name, LONG_STRING_SIZE, "%s", "");
		releaseInstructorLock();
		return;
	}
	showScene(new_scene);
	current_scene = new_scene;
	simmgr_shm->status.scenario.elapsed_msec_scene = 0;
	cprCumulative = 0;
	cprActive = 0;
	simmgr_shm->status.cpr.duration = 0;

	sprintf_s(simmgr_shm->status.scenario.scene_name, LONG_STRING_SIZE, "%s", current_scene->name);

	simmgr_shm->status.scenario.scene_id = sceneId;
	simmgr_shm->status.respiration.manual_count = 0;

	if (current_scene->id <= 0)
	{
		//if (verbose)
		{
			printf("End scene %s\n", current_scene->name);
		}
		snprintf(s_msg, MAX_MSG_SIZE, "Scenario: End Scene %d %s", sceneId, current_scene->name);
		takeInstructorLock();
		addComment(s_msg);
		sprintf_s(simmgr_shm->instructor.scenario.state, NORMAL_STRING_SIZE, "%s", "Terminate");
		releaseInstructorLock();
	}
	else
	{
		//if (verbose)
		{
			printf("New scene %s\n", current_scene->name);
		}
		snprintf(s_msg, MAX_MSG_SIZE, "Scenario: Start Scene %d: %s", sceneId, current_scene->name);
		lockAndComment(s_msg);
		
		// Clear Events
		simmgr_shm->eventListNextWrite = 0;
		simmgr_shm->eventListNextRead = 0;
		memset(simmgr_shm->eventList, 0, sizeof(simmgr_shm->eventList));

		processInit(&current_scene->initParams);
		// Clear completion counts in any trigger groups
		struct trigger_group * trigger_group;
		struct scenario_trigger* trigger;

		trigger_group = (struct trigger_group* )current_scene->group_list.next;
		while (trigger_group)
		{
			trigger_group->group_triggers_met = 0;
			trigger = (struct scenario_trigger*)trigger_group->group_list.next;
			while (trigger)
			{
				trigger->met = 0;
				trigger = (struct scenario_trigger*)trigger->trigger_list.next;
			}
			trigger_group = (struct trigger_group*)trigger_group->group_list.next;
		}
	}
}
