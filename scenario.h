#pragma once
/*
 * scenario.h
 *
 * Scenario Processing
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

#ifndef _SCENARIO_H
#define _SCENARIO_H

#include "llist.h"

#define SCENARIO_LOOP_DELAY	250	// Sleep time (msec) for the Scenario loop
#define LONG_STRING_SIZE	128
#define NORMAL_STRING_SIZE	32
#define SCENE_TITLE_MAX		32

#define PARSE_STATE_NONE	0
#define PARSE_STATE_INIT	1
#define PARSE_STATE_SCENE	2
#define PARSE_STATE_EVENTS	3
#define PARSE_STATE_HEADER	4

#define PARSE_INIT_STATE_NONE			0
#define PARSE_INIT_STATE_CARDIAC		1
#define PARSE_INIT_STATE_RESPIRATION	2
#define PARSE_INIT_STATE_GENERAL		3
#define PARSE_INIT_STATE_SCENE			4
#define PARSE_INIT_STATE_VOCALS			5
#define PARSE_INIT_STATE_MEDIA			6
#define PARSE_INIT_STATE_CPR			7
#define PARSE_INIT_STATE_TELESIM		8

#define PARSE_SCENE_STATE_NONE				0
#define PARSE_SCENE_STATE_INIT				1
#define PARSE_SCENE_STATE_INIT_CARDIAC		2
#define PARSE_SCENE_STATE_INIT_RESPIRATION	3
#define PARSE_SCENE_STATE_INIT_GENERAL		4
#define PARSE_SCENE_STATE_INIT_VOCALS		5
#define PARSE_SCENE_STATE_INIT_MEDIA		6
#define PARSE_SCENE_STATE_INIT_CPR			7
#define PARSE_SCENE_STATE_INIT_TELESIM		8
#define PARSE_SCENE_STATE_TIMEOUT			20
#define PARSE_SCENE_STATE_TRIGS				21
#define PARSE_SCENE_STATE_TRIG				22

#define PARSE_HEADER_STATE_NONE				0
#define PARSE_HEADER_STATE_AUTHOR			1
#define PARSE_HEADER_STATE_TITLE			2
#define PARSE_HEADER_STATE_DATE_OF_CREATION	3
#define PARSE_HEADER_STATE_DESCRIPTION		4

// Scenario
struct scenario_data
{
	char author[LONG_STRING_SIZE+2];
	char title[LONG_STRING_SIZE+2];
	char date_created[NORMAL_STRING_SIZE+2];
	char description[LONG_STRING_SIZE+2];

	// Initialization Parameters for the scenario
	struct instructor initParams;

	struct snode scene_list;
	struct snode event_list;
};

struct scenario_scene
{
	struct snode scene_list;
	int id;				// numeric ID - 1 is always the entry scene, 2 is always the end scene
	char name[LONG_STRING_SIZE+1];		// 

	// Initialization Parameters for the scene
	struct instructor initParams;

	// Timeout in Seconds
	int timeout;
	int timeout_scene;

	struct snode trigger_list;
	int triggers_needed;
};



// A trigger is defined as a setting of a parameter, or the setting of a trend. A trend time of 0 indicates immediate.

#define TRIGGER_NAME_LENGTH 	32
#define TRIGGER_TEST_EQ			0
#define TRIGGER_TEST_LTE		1
#define TRIGGER_TEST_LT			2
#define TRIGGER_TEST_GTE		3
#define TRIGGER_TEST_GT			4
#define TRIGGER_TEST_INSIDE		5
#define TRIGGER_TEST_OUTSIDE	6
#define TRIGGER_TEST_EVENT		7	// Special - Wait for Event Injection from Instructor (or mannequin )

// Note: When Test is TRIGGER_TEST_EVENT, the param_element is the event_id

struct scenario_trigger
{
	struct	snode trigger_list;
	char 	param_class[TRIGGER_NAME_LENGTH+2];	// Class eg: cardiac, resipration, ...
	char 	param_element[TRIGGER_NAME_LENGTH+2];	// Parameter eg: rate, transfer_time, ...
	int		test;
	int		value;		// Comparison value
	int		value2;		// Comparison value (only for Inside/Outside)
	int 	scene;		// ID of next scene
	int		group;		// Set to include in group
};

struct scenario_event
{
	struct	snode event_list;
	char	event_catagory_name[NORMAL_STRING_SIZE+2];
	char	event_catagory_title[NORMAL_STRING_SIZE+2];
	char	event_title[NORMAL_STRING_SIZE+2];
	char	event_id[NORMAL_STRING_SIZE+2];
};

// For Parsing the XML:
#define PARAMETER_NAME_LENGTH	128
struct xml_level
{
	int num;
	char name[PARAMETER_NAME_LENGTH];
};

#endif // _SCENARIO_H