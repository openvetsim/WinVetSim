/*
 * scenario_xml.cpp
 *
 * Scenario Processing
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSim/sim-mgr).
 *
 * Copyright (c) 2019-2025 ITown Design
 * Copyright (c) 2019-2021 Cornell University College of Veterinary Medicine Ithaca, NY
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
 * The scenario script is an XML formatted file. It is parsed by the scenario process to
 * define the various scenes. A scene contains a set of initialization parameters and
 * a set a triggers.
 *
 */
#include "vetsim.h"
#include "scenario.h"
#include "llist.h"
#include "XMLRead.h"

XMLRead xmlr;

extern int current_scene_id;
struct xml_level xmlLevels[10];
extern int xml_current_level;

extern const char* xml_filename;
extern int line_number;
extern int verbose;
extern int checkOnly;
extern int errCount;

extern std::wstring parseLog;
extern int parse_state;
extern int parse_init_state;
extern int parse_scene_state;
extern int parse_header_state;

const char* parse_states[] =
{
	"PARSE_STATE_NONE", "PARSE_STATE_INIT", "PARSE_STATE_SCENE",
	"PARSE_STATE_HEADER"
};

const char* parse_init_states[] =
{
	"NONE", "CARDIAC", "RESPIRATION",
	"GENERAL", "SCENE", "VOCALS", "TELESIM", "CPR"
};

const char* parse_scene_states[] =
{
	"NONE", "INIT", "CARDIAC", "RESPIRATION",
	"GENERAL", "VOCALS", "MEDIA", "CPR", "TELESIM",
	"TIMEOUT", "TRIGS", "TRIG", "TRIG_GROUP", "TRIG_GROUP_TRIG"
};

const char* parse_header_states[] =
{
	"NONE", "AUTHOR", "TITLE", "DATE_OF_CREATION",
	"DESCRIPTION"
};

const char* trigger_tests[] =
{
	"EQ", "LTE", "LT", "GTE", "GT", "INSIDE", "OUTSIDE", "EVENT"
};

const char* trigger_tests_sym[] =
{
	"==", "<=", "<", ">=", ">", "", "", ""
};

char current_event_catagory[NORMAL_STRING_SIZE + 2];
char current_event_title[NORMAL_STRING_SIZE + 2];

extern struct scenario_scene* current_scene;
extern struct scenario_data* scenario;
struct scenario_scene* new_scene;
struct scenario_trigger* new_trigger;
struct trigger_group* new_trigger_group;
struct scenario_event* new_event;

/**
 *  appendToParseLog
 * @strptr
 *
 * Append the string to the pareseLog
*/
void
appendToParseLog(char* str)
{
	::std::wstring wideStr;
	int convertResult = MultiByteToWideChar(CP_UTF8, 0, simmgr_shm->status.scenario.error_message, (int)strlen(simmgr_shm->status.scenario.error_message), NULL, 0);
	if (convertResult > 0)
	{
		wideStr.resize(convertResult + 10);
		convertResult = MultiByteToWideChar(CP_UTF8, 0, simmgr_shm->status.scenario.error_message, (int)strlen(simmgr_shm->status.scenario.error_message), &wideStr[0], (int)wideStr.size());
		parseLog.append(wideStr);
	}
}

/**
 *  scanForDuplicateScene
 * @scene_id
 *
 * Returns the number of matches found.
*/
//std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

int
scanForDuplicateScene(int sceneId)
{
	struct snode* snode;
	struct scenario_scene* scene;
	int match = 0;

	snode = scenario->scene_list.next;

	while (snode)
	{
		scene = (struct scenario_scene*)snode;
		if (scene->id == sceneId)
		{
			printf("sfds: %d %d\n", scene->id, sceneId);
			match++;
		}
		snode = get_next_llist(snode);
	}
	if (match < 1)
	{
		printf("ERROR: Scene ID %d not found\n", sceneId);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Scene ID %d not found\n", sceneId);
		appendToParseLog(simmgr_shm->status.scenario.error_message);

		errCount++;
	}
	else if (match > 1)
	{
		printf("ERROR: duplicate check, Scene ID %d found %d times\n", sceneId, match);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: DUPLICATE Scene ID %d found %d times\n", sceneId, match);
		appendToParseLog(simmgr_shm->status.scenario.error_message);
		errCount++;
	}
	return (match);
}
/**
 *  scanForDuplicateEvent
 * @scene_id
 *
 * Returns the number of matches found.
*/

int
scanForDuplicateEvent(char* eventId)
{
	struct snode* snode;
	struct scenario_event* event;
	int match = 0;

	snode = scenario->event_list.next;

	while (snode)
	{
		event = (struct scenario_event*)snode;
		if (strcmp(event->event_id, eventId) == 0)
		{
			match++;
		}
		snode = get_next_llist(snode);
	}
	if (match < 1)
	{
		printf("ERROR: Event ID %s not found\n", eventId);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Event ID %s not found\n", eventId);
		appendToParseLog(simmgr_shm->status.scenario.error_message);
		errCount++;
	}
	else if (match > 1)
	{
		printf("ERROR: duplicate check, Event ID %s found %d times\n", eventId, match);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: duplicate check, Event ID %s found %d times\n", eventId, match);
		appendToParseLog(simmgr_shm->status.scenario.error_message);
		errCount++;
	}
	return (match);
}


/**
 *  showScenes
 * @scene_id
 *
*/
struct scenario_scene*
showScenes()
{
	struct snode* snode;
	struct scenario_scene* scene;
	struct scenario_trigger* trig;
	struct scenario_event* event;
	struct snode* t_snode;
	struct snode* e_snode;
	struct snode* g_snode;
	int duplicates;
	int tcount;
	int timeout = 0;

	snode = scenario->scene_list.next;

	while (snode)
	{
		scene = (struct scenario_scene*)snode;
		printf("Scene %d: %s\n", scene->id, scene->name);
		//if (scene->id < 0)
		//{
		//	printf("ERROR: Scene ID %d is invalid\n",
		//		scene->id);
		//	snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Scene ID %d is invalid\n",
		//		scene->id);
		//	appendToParseLog(simmgr_shm->status.scenario.error_message);
		//	errCount++;
		//}
		duplicates = scanForDuplicateScene(scene->id);
		if (duplicates != 1)
		{
			printf("ERROR: Scene ID %d has %d duplicate entries\n",
				scene->id, duplicates);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Scene ID %d has %d entries\n",
				scene->id, duplicates);
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		tcount = 0;
		timeout = 0;
		if (scene->timeout > 0)
		{
			printf("\tTimeout: %d Scene %d\n", scene->timeout, scene->timeout_scene);
			timeout++;
		}
		printf("\tGroup Triggers:\n");
		g_snode = scene->group_list.next;
		if (!g_snode)
		{
			printf("No Group Triggers Found\n");
		}
		while (g_snode)
		{
			struct trigger_group* trig_group = (struct trigger_group*)g_snode;
			printf("\t\tNeeded: %d,  Next Scene %d \n", trig_group->group_triggers_needed, trig_group->scene);
			trig = (struct scenario_trigger*)trig_group->group_trigger_list.next;
			while (trig)
			{
				if (trig->test == TRIGGER_TEST_EVENT)
				{
					printf("\t\t\tEvent : %s\n", trig->param_element);
				}
				else
				{
					printf("\t\t\tTrig %s %s %s %d %d %d %d\n",
						trig->param_class, trig->param_element,
						trigger_tests[trig->test],
						trig->value,
						trig->value2,
						trig->scene,
						trig->group);
				}
				trig = (struct scenario_trigger*)trig->trigger_list.next;
			}
			g_snode = get_next_llist(g_snode);
		}

		printf("\tTriggers:\n");
		t_snode = scene->trigger_list.next;
		while (t_snode)
		{
			tcount++;
			trig = (struct scenario_trigger*)t_snode;
			if (trig->test == TRIGGER_TEST_EVENT)
			{
				printf("\t\tEvent : %s\n", trig->param_element);
			}
			else
			{
				printf("\t\t%s:%s, %s, %d, %d - Scene %d\n",
					trig->param_class, trig->param_element, trigger_tests[trig->test], trig->value, trig->value2, trig->scene);
			}
			t_snode = get_next_llist(t_snode);
		}
		/*
		if ((scene->id != 0) && (tcount == 0) && (timeout == 0))
		{
			printf("ERROR: Scene ID %d has no trigger/timeout events\n",
				scene->id);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Scene ID %d has no trigger/timeout events\n",
				scene->id );
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		if ((scene->id == 0) && (tcount != 0) && (timeout != 0))
		{
			printf("ERROR: End Scene ID %d has %d triggers %d timeouts. Should be none.\n",
				scene->id, tcount, timeout);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: End Scene ID %d has %d triggers %d timeouts. Should be none.\n",
				scene->id, tcount, timeout);
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		*/
		snode = get_next_llist(snode);
	}
	printf("Events:\n");
	e_snode = scenario->event_list.next;
	while (e_snode)
	{
		event = (struct scenario_event*)e_snode;
		printf("\t'%s'\t'%s'\t'%s'\t'%s'\n",
			event->event_catagory_name, event->event_catagory_title, event->event_title, event->event_id);
		duplicates = scanForDuplicateEvent(event->event_id);
		if (duplicates != 1)
		{
			printf("ERROR: Event ID %s has %d entries\n",
				event->event_id, duplicates);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Event ID %s has %d entries\n",
				event->event_id, duplicates);
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		e_snode = get_next_llist(e_snode);
	}
	return (NULL);
}

/**
 *  validateScenes
 * @scene_id
 *
*/
int
validateScenes()
{
	struct snode* snode;
	struct scenario_scene* scene;
	struct scenario_trigger* trig;
	struct scenario_event* event;
	struct snode* t_snode;
	struct snode* e_snode;
	int duplicates;
	int tcount;
	int timeout = 0;

	snode = scenario->scene_list.next;

	while (snode)
	{
		scene = (struct scenario_scene*)snode;
		if (scene->id < 0)
		{
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "Scenario ERROR: Scene ID % d is invalid\n",
				scene->id);
			return (-1);
		}
		duplicates = scanForDuplicateScene(scene->id);
		if (duplicates != 1)
		{
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "Scenario ERROR: Scene ID %d has duplicates in XML file\n",
				scene->id);
			return (-1);
		}
		tcount = 0;
		timeout = 0;
		if (scene->timeout > 0)
		{
			printf("\tTimeout: %d Scene %d\n", scene->timeout, scene->timeout_scene);
			timeout++;
		}
		printf("\tTriggers:\n");
		t_snode = scene->trigger_list.next;
		while (t_snode)
		{
			tcount++;
			trig = (struct scenario_trigger*)t_snode;
			if (trig->test == TRIGGER_TEST_EVENT)
			{
				printf("\t\tEvent : %s\n", trig->param_element);
			}
			else
			{
				printf("\t\t%s:%s, %s, %d, %d - Scene %d\n",
					trig->param_class, trig->param_element, trigger_tests[trig->test], trig->value, trig->value2, trig->scene);
			}
			t_snode = get_next_llist(t_snode);
		}
		if ((scene->id != 0) && (tcount == 0) && (timeout == 0))
		{
			printf("ERROR: Scene ID %d has no trigger/timeout events\n",
				scene->id);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: Event ID %d has no trigger/timeout events\n",
				scene->id);
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		if ((scene->id == 0) && (tcount != 0) && (timeout != 0))
		{
			printf("ERROR: End Scene ID %d has %d triggers %d timeouts. Should be none.\n",
				scene->id, tcount, timeout);
			snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: End Scene ID %d has %d triggers %d timeouts. Should be none.\n",
				scene->id, tcount, timeout);
			appendToParseLog(simmgr_shm->status.scenario.error_message);
			errCount++;
		}
		snode = get_next_llist(snode);
	}
	printf("Events:\n");
	e_snode = scenario->event_list.next;
	while (e_snode)
	{
		event = (struct scenario_event*)e_snode;
		printf("\t'%s'\t'%s'\t'%s'\t'%s'\n",
			event->event_catagory_name, event->event_catagory_title, event->event_title, event->event_id);
		duplicates = scanForDuplicateEvent(event->event_id);
		if (duplicates != 1)
		{
			printf("ERROR: Event ID %s has %d entries\n",
				event->event_id, duplicates);
			return (-1);
		}
		e_snode = get_next_llist(e_snode);
	}
	return (NULL);
}

/**
 * saveData:
 * @xmlName: name of the entry
 * @xmlValue: Text data to convert and save in structure
 *
 * Take the current Text entry and save as appropriate in the currently
 * named data structure
*/

static void
saveData(const char* xmlName, const char* xmlValue)
{
	char* name = (char*)xmlName;
	char* value = (char*)xmlValue;
	int sts = 0;
	int i;
	char* value2;
	char complex[1024];

	switch (parse_state)
	{
	case PARSE_STATE_NONE:
		if (verbose)
		{
			printf("STATE_NONE: Lvl %d Name %s, Value, %s\n",
				xml_current_level, xmlLevels[xml_current_level].name, value);
		}
		break;
	case PARSE_STATE_INIT:
		switch (parse_init_state)
		{
		case PARSE_INIT_STATE_NONE:
			if (verbose)
			{
				printf("INIT_NONE: Lvl %d Name %s, Value, %s\n",
					xml_current_level, xmlLevels[xml_current_level].name, value);
			}
			sts = 0;
			break;

		case PARSE_INIT_STATE_CARDIAC:
			if (xml_current_level == 3)
			{
				sts = cardiac_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.cardiac);
			}
			break;
		case PARSE_INIT_STATE_RESPIRATION:
			if (xml_current_level == 3)
			{
				sts = respiration_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.respiration);
			}
			break;
		case PARSE_INIT_STATE_GENERAL:
			if (xml_current_level == 3)
			{
				sts = general_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.general);
			}
			break;
		case PARSE_INIT_STATE_TELESIM:
			if (xml_current_level == 3)
			{
				sts = telesim_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.telesim);
			}
			else if (xml_current_level == 4)
			{
				sprintf_s(complex, 1024, "%s:%s", xmlLevels[3].name, value);
				sts = telesim_parse(xmlLevels[xml_current_level].name, complex, &scenario->initParams.telesim);
			}
			break;
		case PARSE_INIT_STATE_VOCALS:
			if (xml_current_level == 3)
			{
				sts = vocals_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.vocals);
			}
			break;
		case PARSE_INIT_STATE_MEDIA:
			if (xml_current_level == 3)
			{
				sts = media_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.media);
			}
			break;
		case PARSE_INIT_STATE_CPR:
			if (xml_current_level == 3)
			{
				sts = cpr_parse(xmlLevels[xml_current_level].name, value, &scenario->initParams.cpr);
			}
			break;
		case PARSE_INIT_STATE_SCENE:
			if ((xml_current_level == 2) &&
				(strcmp(xmlLevels[xml_current_level].name, "initial_scene") == 0))
			{
				simmgr_shm->status.scenario.scene_id = atoi(value);
				current_scene_id = simmgr_shm->status.scenario.scene_id;
				if (verbose)
				{
					printf("Set Initial Scene to ID %d\n", current_scene_id);
				}
				sts = 0;
			}
			else if ((xml_current_level == 2) &&
				(strcmp(xmlLevels[xml_current_level].name, "scene") == 0))
			{
				simmgr_shm->status.scenario.scene_id = atoi(value);
				current_scene_id = simmgr_shm->status.scenario.scene_id;
				if (verbose)
				{
					printf("Set Scene to ID %d\n", current_scene_id);
				}
				sts = 0;
			}
			break;
		default:
			sts = 0;
			break;
		}
		break;

	case PARSE_STATE_SCENE:
		switch (parse_scene_state)
		{
		case PARSE_SCENE_STATE_NONE:
			if (verbose)
			{
				printf("SCENE_STATE_NONE: Lvl %d Name %s, Value, %s\n",
					xml_current_level, xmlLevels[xml_current_level].name, value);
			}

			if (xml_current_level == 2)
			{
				if (strcmp(xmlLevels[2].name, "id") == 0)
				{
					new_scene->id = atoi(value);
					if (verbose)
					{
						printf("Set Scene ID to %d\n", new_scene->id);
					}
				}
				else if (strcmp(xmlLevels[2].name, "triggers_needed") == 0)
				{
					snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "ERROR: In Scene %d, 'triggers_needed' found.\n",
						new_scene->id);
					appendToParseLog(simmgr_shm->status.scenario.error_message);
					appendToParseLog((char*)"See 'https://vetsim.net/groupTriggers.php'\n");
					errCount++;
					printf("Error Triggers Needed found.");
					//new_scene->triggers_needed = atoi(value);
					//printf("Set Triggers Needed to %d\n", new_scene->triggers_needed);
				}
				else if (strcmp(xmlLevels[2].name, "title") == 0)
				{
					// Truncate the scene Title to prevent overflow on II screen
					if (strlen(value) > SCENE_TITLE_MAX)
					{
						value[SCENE_TITLE_MAX] = 0;
					}
					sprintf_s(new_scene->name, LONG_STRING_SIZE, "%s", value);
					if (verbose)
					{
						printf("Set Scene Name to %s\n", new_scene->name);
					}
				}
			}
			sts = 0;
			break;

		case PARSE_SCENE_STATE_TIMEOUT:
			if (xml_current_level == 3)
			{
				if (strcmp(xmlLevels[3].name, "timeout_value") == 0)
				{
					new_scene->timeout = atoi(value);
				}
				else if (strcmp(xmlLevels[3].name, "scene_id") == 0)
				{
					new_scene->timeout_scene = atoi(value);
				}
			}
			break;


		case PARSE_SCENE_STATE_INIT_CARDIAC:
			if (xml_current_level == 4)
			{
				sts = cardiac_parse(xmlLevels[4].name, value, &new_scene->initParams.cardiac);
			}
			break;
		case PARSE_SCENE_STATE_INIT_RESPIRATION:
			if (xml_current_level == 4)
			{
				sts = respiration_parse(xmlLevels[4].name, value, &new_scene->initParams.respiration);
			}
			break;
		case PARSE_SCENE_STATE_INIT_GENERAL:
			if (xml_current_level == 4)
			{
				sts = general_parse(xmlLevels[4].name, value, &new_scene->initParams.general);
			}
			break;
		case PARSE_SCENE_STATE_INIT_TELESIM:
			if (xml_current_level == 5)
			{
				sprintf_s(complex, 1024, "%s:%s", xmlLevels[4].name, value);
				sts = telesim_parse(xmlLevels[xml_current_level].name, complex, &new_scene->initParams.telesim);
			}
			else if (xml_current_level == 4)
			{
				sprintf_s(complex, 1024, "%s:%s", xmlLevels[3].name, value);
				sts = telesim_parse(xmlLevels[xml_current_level].name, complex, &new_scene->initParams.telesim);
			}
			break;
		case PARSE_SCENE_STATE_INIT_VOCALS:
			if (xml_current_level == 4)
			{
				sts = vocals_parse(xmlLevels[4].name, value, &new_scene->initParams.vocals);
			}
			break;
		case PARSE_SCENE_STATE_INIT_MEDIA:
			if (xml_current_level == 4)
			{
				sts = media_parse(xmlLevels[4].name, value, &new_scene->initParams.media);
			}
			break;
		case PARSE_SCENE_STATE_INIT_CPR:
			if (xml_current_level == 4)
			{
				sts = cpr_parse(xmlLevels[4].name, value, &new_scene->initParams.cpr);
			}
			break;
		case PARSE_SCENE_STATE_TRIGS:
			break;
		case PARSE_SCENE_STATE_TRIG_GROUP:
			if (xml_current_level == 4)
			{
				if (strcmp(xmlLevels[4].name, "scene_id") == 0)
				{
					new_trigger_group->scene = atoi(value);
				}
				else if (strcmp(xmlLevels[4].name, "triggers_required") == 0)
				{
					new_trigger_group->group_triggers_needed = atoi(value);
				}
				else if (strcmp(xmlLevels[4].name, "group_id") == 0)
				{
					new_trigger_group->group_id = atoi(value);
				}
			}
			break;
		case PARSE_SCENE_STATE_TRIG:
			if (xml_current_level == 4)
			{
				if (strcmp(xmlLevels[4].name, "test") == 0)
				{
					for (i = 0; i <= TRIGGER_TEST_OUTSIDE; i++)
					{
						if (strcmp(trigger_tests[i], value) == 0)
						{
							new_trigger->test = i;
							break;
						}
					}
				}
				else if (strcmp(xmlLevels[4].name, "scene_id") == 0)
				{
					new_trigger->scene = atoi(value);
				}
				else if (strcmp(xmlLevels[4].name, "event") == 0)
				{
					sprintf_s(new_trigger->param_element, 32, "%s", value);
					new_trigger->test = TRIGGER_TEST_EVENT;
				}
				//else if (strcmp(xmlLevels[4].name, "group") == 0)
				//{
				//	new_trigger->group = atoi(value);
				//}
				else
				{
					sts = 1;
				}
			}
			else if (xml_current_level == 5)
			{
				sprintf_s(new_trigger->param_class, 32, "%s", xmlLevels[4].name);
				sprintf_s(new_trigger->param_element, 32, "%s", xmlLevels[5].name);
				new_trigger->value = atoi(value);

				// For range, the two values are shown as "2-4". No spaces allowed.
				value2 = strchr(value, '-');
				if (value2)
				{
					value2++;
					new_trigger->value2 = atoi(value2);
				}
				if (verbose)
				{
					printf(" Trig In Group Added: %s : %s : %d-%d\n",
						new_trigger->param_class, new_trigger->param_element,
						new_trigger->value, new_trigger->value2);
				}
			}
			else
			{
				sts = 2;
			}
			break;

		case PARSE_SCENE_STATE_TRIG_GROUP_TRIG:
			if (xml_current_level == 5)
			{
				if (strcmp(xmlLevels[5].name, "test") == 0)
				{
					for (i = 0; i <= TRIGGER_TEST_OUTSIDE; i++)
					{
						if (strcmp(trigger_tests[i], value) == 0)
						{
							new_trigger->test = i;
							break;
						}
					}
				}
				else if (strcmp(xmlLevels[5].name, "scene_id") == 0)
				{
					new_trigger->scene = atoi(value);
				}
				else if (strcmp(xmlLevels[5].name, "event") == 0)
				{
					sprintf_s(new_trigger->param_element, 32, "%s", value);
					new_trigger->test = TRIGGER_TEST_EVENT;
				}
				else if (strcmp(xmlLevels[5].name, "group_id") == 0)
				{
					new_trigger->group = atoi(value);
				}
				else
				{
					sts = 1;
				}
			}
			else if (xml_current_level == 6)
			{
				sprintf_s(new_trigger->param_class, 32, "%s", xmlLevels[5].name);
				sprintf_s(new_trigger->param_element, 32, "%s", xmlLevels[6].name);
				new_trigger->value = atoi(value);

				// For range, the two values are shown as "2-4". No spaces allowed.
				value2 = strchr(value, '-');
				if (value2)
				{
					value2++;
					new_trigger->value2 = atoi(value2);
				}
				if (verbose)
				{
					printf("%s : %s : %d-%d\n",
						new_trigger->param_class, new_trigger->param_element,
						new_trigger->value, new_trigger->value2);
				}
			}
			else
			{
				sts = 2;
			}
			break;

		case PARSE_SCENE_STATE_INIT:
		default:
			if (verbose)
			{
				printf("SCENE_STATE default (%d): Lvl %d Name %s, Value, %s\n",
					parse_scene_state, xml_current_level, xmlLevels[xml_current_level].name, value);
			}
			sts = -1;
			break;
		}
		break;

	case PARSE_STATE_EVENTS:
		if (xml_current_level == 3)
		{
			if (strcmp(xmlLevels[3].name, "name") == 0)
			{
				// Set the current Category Name
				sprintf_s(current_event_catagory, NORMAL_STRING_SIZE, "%s", value);
			}
			else if (strcmp(xmlLevels[3].name, "title") == 0)
			{
				// Set the current Category Name
				sprintf_s(current_event_title, NORMAL_STRING_SIZE, "%s", value);
			}
		}
		else if (xml_current_level == 4)
		{
			if (strcmp(xmlLevels[4].name, "title") == 0)
			{
				sprintf_s(new_event->event_title, 32, "%s", value);
			}
			else if (strcmp(xmlLevels[4].name, "id") == 0)
			{
				sprintf_s(new_event->event_id, 32, "%s", value);
			}
		}
		break;
	case PARSE_STATE_HEADER:
		if (xml_current_level == 2)
		{
			if (strcmp(xmlLevels[2].name, "author") == 0)
			{
				snprintf(scenario->author, LONG_STRING_SIZE, "%s", value);
			}
			else if (strcmp(xmlLevels[2].name, "date_created") == 0)
			{
				snprintf(scenario->date_created, NORMAL_STRING_SIZE, "%s", value);
			}
			else if (strcmp(xmlLevels[2].name, "description") == 0)
			{
				snprintf(scenario->description, LONG_STRING_SIZE, "%s", value);
			}
		}
		else if (xml_current_level == 3)
		{
			if (strcmp(xmlLevels[3].name, "title") == 0)
			{
				snprintf(scenario->title, LONG_STRING_SIZE, "%s", value);
			}
		}
		[[fallthrough]];
	default:
		if (strcmp(xmlLevels[1].name, "header") == 0)
		{
			if (xml_current_level == 2)
			{
				name = xmlLevels[2].name;
				if (strcmp(name, "author") == 0)
				{
					sprintf_s(scenario->author, 128, "%s", value);
					if (verbose)
					{
						printf("Author: %s\n", scenario->author);
					}
				}
				else if (strcmp(name, "date_of_creation") == 0)
				{
					snprintf(scenario->date_created, NORMAL_STRING_SIZE, "%s", value);
					if (verbose)
					{
						printf("Created: %s\n", scenario->date_created);
					}
				}
				else if (strcmp(name, "description") == 0)
				{
					snprintf(scenario->description, LONG_STRING_SIZE, "%s", value);
					if (verbose)
					{
						printf("Description: %s\n", scenario->description);
					}
				}
				else
				{
					if (verbose)
					{
						printf("In Header, Unhandled: Name is %s, Value, %s\n", xmlLevels[xml_current_level].name, value);
					}
				}
			}
			else if (xml_current_level == 3)
			{
				name = xmlLevels[3].name;
				if (strcmp(name, "name") == 0)
				{
					snprintf(scenario->title, LONG_STRING_SIZE, "%s", value);
					if (verbose)
					{
						printf("Title: %s\n", scenario->title);
					}
				}
			}
			else
			{
				if (verbose)
				{
					printf("In Header, Level is %d, Name is %s, Value, %s\n", xml_current_level, xmlLevels[xml_current_level].name, value);
				}
			}
		}
		break;
	}
	if (sts && verbose)
	{
		printf("saveData STS %d: Lvl %d: %s, Value  %s, \n", sts, xml_current_level, xmlLevels[xml_current_level].name, value);
	}
}

/**
 * startParseState:
 * @lvl: New level from XML
 * @name: Name of the new level
 *
 * Process level change.
*/

static void
startParseState(int lvl, char* name)
{
	if (!name)
	{
		printf("startParseState called with null name\n");
		return;
	}
	if (verbose)
	{
		if (name)
		{
			printf("startParseState: Cur State %s: Lvl %d Name %s\t", parse_states[parse_state], lvl, name);
		}
		else
		{
			printf("startParseState: Cur State %s: Lvl %d Name NULL\t", parse_states[parse_state], lvl);
		}
		if (parse_state == PARSE_STATE_INIT)
		{
			printf(" %s", parse_init_states[parse_init_state]);
		}
		else if (parse_state == PARSE_STATE_SCENE)
		{
			printf(" (%s)", parse_scene_states[parse_scene_state]);
		}
		printf("\n");
	}
	switch (lvl)
	{
	case 0:		// Top level - no actions
		break;

	case 1:	// profile, media, events have no action
		if (strcmp(name, "init") == 0)
		{
			parse_state = PARSE_STATE_INIT;
		}
		else if ((strcmp(name, "scene") == 0) || (strcmp(name, "initial_scene") == 0))
		{
			// Allocate a scene
			new_scene = (struct scenario_scene*)calloc(1, sizeof(struct scenario_scene));
			if (new_scene)
			{
				initializeParameterStruct(&new_scene->initParams);
				insert_llist(&new_scene->scene_list, &scenario->scene_list);
				parse_state = PARSE_STATE_SCENE;
				if (verbose)
				{
					printf("***** New Scene started ******\n");
				}
			}
			else
			{
				printf("Failed to calloc new_scene\n");
			}
		}
		else if (strcmp(name, "events") == 0)
		{
			parse_state = PARSE_STATE_EVENTS;
			sprintf_s(current_event_catagory, NORMAL_STRING_SIZE, "%s", "");
			sprintf_s(current_event_title, NORMAL_STRING_SIZE, "%s", "");
		}
		else if (strcmp(name, "header") == 0)
		{
			parse_state = PARSE_STATE_HEADER;
			sprintf_s(current_event_catagory, NORMAL_STRING_SIZE, "%s", "");
			sprintf_s(current_event_title, NORMAL_STRING_SIZE, "%s", "");
		}
		break;

	case 2:	// 
		switch (parse_state)
		{
		case PARSE_STATE_INIT:
			if (strcmp(name, "cardiac") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_CARDIAC;
			}
			else if (strcmp(name, "respiration") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_RESPIRATION;
			}
			else if (strcmp(name, "general") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_GENERAL;
			}
			else if (strcmp(name, "vocals") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_VOCALS;
			}
			else if (strcmp(name, "media") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_MEDIA;
			}
			else if (strcmp(name, "cpr") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_CPR;
			}
			else if (strcmp(name, "scene") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_SCENE;
			}
			else if (strcmp(name, "initial_scene") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_SCENE;
			}
			else if (strcmp(name, "telesim") == 0)
			{
				parse_init_state = PARSE_INIT_STATE_TELESIM;
			}
			else
			{
				parse_init_state = PARSE_INIT_STATE_NONE;
			}
			break;

		case PARSE_STATE_SCENE:
			if (strncmp(name, "init", 4) == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT;
			}
			else if (strncmp(name, "timeout", 7) == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_TIMEOUT;
			}
			else if (strcmp(name, "triggers_needed") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_NONE;
				printf("Name %s - \n", name);
			}
			else if (strncmp(name, "trigger_group", 13) == 0)
			{
				printf("PARSE_SCENE_STATE_TRIG_GROUP\n");
				parse_scene_state = PARSE_SCENE_STATE_TRIG_GROUP;
			}
			else if (strncmp(name, "triggers", 8) == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_TRIGS;
			}
			else
			{
				parse_scene_state = PARSE_SCENE_STATE_NONE;
			}
			break;

		case PARSE_STATE_HEADER:
			if (strcmp(name, "author") == 0)
			{
				parse_header_state = PARSE_HEADER_STATE_AUTHOR;
			}
			else if (strcmp(name, "title") == 0)
			{
				parse_header_state = PARSE_HEADER_STATE_TITLE;
			}
			else if (strcmp(name, "date_of_creation") == 0)
			{
				parse_header_state = PARSE_HEADER_STATE_DATE_OF_CREATION;
			}
			else if (strcmp(name, "description") == 0)
			{
				parse_header_state = PARSE_HEADER_STATE_DESCRIPTION;
			}
			break;

		default:
			break;
		}
		break;

	case 3:
		switch (parse_state)
		{
		case PARSE_STATE_SCENE:
			if (strcmp(name, "cardiac") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_CARDIAC;
			}
			else if (strcmp(name, "respiration") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_RESPIRATION;
			}
			else if (strcmp(name, "general") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_GENERAL;
			}
			else if (strcmp(name, "vocals") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_VOCALS;
			}
			else if (strcmp(name, "media") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_MEDIA;
			}
			else if (strcmp(name, "cpr") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_CPR;
			}
			else if (strcmp(name, "telesim") == 0)
			{
				parse_scene_state = PARSE_SCENE_STATE_INIT_TELESIM;
			}

			if (parse_scene_state == PARSE_SCENE_STATE_TRIGS)
			{

				if (strcmp(name, "trigger_group") == 0)
				{
					printf("PARSE_SCENE_STATE_TRIG_GROUP\n");
					new_trigger_group = (struct trigger_group*)calloc(1, sizeof(struct trigger_group));
					if (new_trigger_group)
					{
						insert_llist(&new_trigger_group->group_list, &new_scene->group_list);
						if (verbose)
						{
							printf("\n***** New Trigger Group started ******\n");
						}
					}
					parse_scene_state = PARSE_SCENE_STATE_TRIG_GROUP;
				}
				else if (strcmp(name, "trigger") == 0)
				{
					new_trigger = (struct scenario_trigger*)calloc(1, sizeof(struct scenario_trigger));
					if (new_trigger)
					{
						insert_llist(&new_trigger->trigger_list, &new_scene->trigger_list);

						parse_scene_state = PARSE_SCENE_STATE_TRIG;
						if (verbose)
						{
							printf("\n***** New Trigger started ******\n");
						}
					}
					else
					{
						printf("Failed to callog new_trigger\n");
					}
				}
			}
			break;

		case PARSE_STATE_EVENTS:
			if (strcmp(name, "event") == 0)
			{
				if (verbose)
				{
					printf("New Event %s : %s\n", current_event_catagory, current_event_title);
				}
				new_event = (struct scenario_event*)calloc(1, sizeof(struct scenario_event));
				if (new_event)
				{
					insert_llist(&new_event->event_list, &scenario->event_list);

					sprintf_s(new_event->event_catagory_name, NORMAL_STRING_SIZE, "%s", current_event_catagory);
					sprintf_s(new_event->event_catagory_title, NORMAL_STRING_SIZE, "%s", current_event_title);

					if (verbose)
					{
						printf("\n***** New Event started ******\n");
					}
				}
				else
				{
					printf("Faild to calloc new_event\n");
				}
			}
			break;
		case PARSE_STATE_HEADER:
		case PARSE_STATE_INIT:
		case PARSE_STATE_NONE:
			break;
		}


		break;
	case 4:
		switch (parse_state)
		{
			case PARSE_STATE_SCENE:
				if (parse_scene_state == PARSE_SCENE_STATE_TRIG_GROUP)
				{
					if (strcmp(name, "trigger") == 0)
					{
						new_trigger = (struct scenario_trigger*)calloc(1, sizeof(struct scenario_trigger));
						if (new_trigger)
						{
							insert_llist(&new_trigger->trigger_list, &new_trigger_group->group_trigger_list);

							parse_scene_state = PARSE_SCENE_STATE_TRIG_GROUP_TRIG;
							if (verbose)
							{
								printf("\n***** New Trigger started in Group ******\n");
							}
						}
						else
						{
							printf("Failed to callog new_trigger\n");
						}
					}
					//if (strcmp(name, "triggers_needed") == 0)
					//{
					//	new_trigger_group->group_triggers_needed = atoi(value);
					//	printf("Trigger Group %d: Set Triggers Needed to %d\n", new_scene->triggers_needed);
					//}
				}
		}
		break;
	case 5:
		switch (parse_state)
		{
			case PARSE_STATE_SCENE:
				if (parse_scene_state == PARSE_SCENE_STATE_TRIG_GROUP_TRIG)
				{

				}
				break;
		}
	default:
		break;
	}
}
static void
endParseState(int lvl)
{
	if (verbose)
	{
		printf("endParseState: Lvl %d    State: %s ", lvl, parse_states[parse_state]);
		if (parse_state == PARSE_STATE_INIT)
		{
			printf("\t%s\n", parse_init_states[parse_init_state]);
		}
		else if (parse_state == PARSE_STATE_SCENE)
		{
			printf("\t%s\n", parse_scene_states[parse_scene_state]);
		}
		else
		{
			printf("\n");
		}
	}
	switch (lvl)
	{
	case 0:	// Parsing Complete
		break;

	case 1:	// Section End
		parse_state = PARSE_STATE_NONE;
		break;

	case 2:
		switch (parse_state)
		{
		case PARSE_STATE_SCENE:
			parse_scene_state = PARSE_SCENE_STATE_NONE;
			break;

		case PARSE_STATE_INIT:
			parse_init_state = PARSE_INIT_STATE_NONE;
			break;
		}
		break;
	case 3:
		if ((parse_scene_state == PARSE_SCENE_STATE_TRIG) &&
			(new_trigger))
		{
			new_trigger = (struct scenario_trigger*)0;
			parse_scene_state = PARSE_SCENE_STATE_TRIGS;
			printf("**** Trigger Complete ****\n");
		}
		else if ((parse_scene_state == PARSE_SCENE_STATE_TRIG_GROUP) &&
			(new_trigger_group))
		{
			new_trigger_group = (struct trigger_group*)0;
			parse_scene_state = PARSE_SCENE_STATE_TRIGS;
			printf("**** Trigger Group Complete ****\n");
		}
		break;
	case 4:
		if ((parse_scene_state == PARSE_SCENE_STATE_TRIG_GROUP_TRIG) &&
			(new_trigger))
		{
			new_trigger = (struct scenario_trigger*)0;
			parse_scene_state = PARSE_SCENE_STATE_TRIG_GROUP;
			printf("**** Trigger In Group Complete ****\n");
		}
		break;
	case 5:

	default:
		break;
	}
}
/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */

void
processNode(void)
{
	int lvl;
	char* name;
	char* value;

	name = &xmlr.name[0];
	value = &xmlr.value[0];

	switch (xmlr.type)
	{
	case XML_TYPE_ELEMENT:
		xml_current_level = xmlr.depth;
		xmlLevels[xml_current_level].num = xml_current_level;
		if (strlen(name) >= PARAMETER_NAME_LENGTH)
		{
			fprintf(stderr, "XML Parse Error: %s: Name %s exceeds Max Length of %d\n",
				xml_filename, name, PARAMETER_NAME_LENGTH - 1);
		}
		else
		{
			sprintf_s(xmlLevels[xml_current_level].name, 128, "%s", name);
		}
		// printf("Start %d %s\n", xml_current_level, name );
		startParseState(xml_current_level, (char*)name);
		break;

	case XML_TYPE_TEXT:
		cleanString((char*)value);
		if (verbose)
		{
			for (lvl = 0; lvl <= xml_current_level; lvl++)
			{
				printf("[%d]%s:", lvl, xmlLevels[lvl].name);
			}
			printf(" %s\n", value);
		}
		saveData(name, value);
		break;

	case XML_TYPE_END_ELEMENT:
		// printf("End %d\n", xml_current_level );
		endParseState(xml_current_level);
		xml_current_level--;
		break;

	case XML_TYPE_NONE:
	case XML_TYPE_FILE_END:
	default:
		if (verbose)
		{
			printf("Node: %d %d %s",
				xmlr.depth,
				xmlr.type,
				name);

			if (value == NULL)
			{
				printf("\n");
			}
			else
			{
				if (strlen(value) > 60)
				{
					printf(" %.80s...\n", value);
				}
				else
				{
					printf(" %s\n", value);
				}
			}
		}
	}
}

/**
 * readScenario:
 * @filename: the file name to parse
 *
 * Parse the scenario
 */

int
readScenario(const char* name)
{
	int sts;
	char filename[1400];
	extern char sessionsPath[];

	sprintf_s(sessionsPath, 1088, "%s\\scenarios", localConfig.html_path);
	sprintf_s(filename, 1400, "%s\\%s\\main.xml", sessionsPath, name);
	sts = xmlr.open(filename);
	if (sts)
	{
		printf("Failure on read of XML File \"%s\"\n", filename);
		snprintf(simmgr_shm->status.scenario.error_message, STR_SIZE, "Failure on read of XML File \"%s\"\n", filename);
		return (-1);
	}
	while (xmlr.getEntry() == 0)
	{
		processNode();
	}

	return (0);
}