/*
 * scenario.cpp
 *
 * Parser for "init" and Instructor commands. Shared by simstatus.cpp and scenario.c
 *
 * This file is part of the sim-mgr distribution (https://github.com/OpenVetSimDevelopers/sim-mgr).
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

#include "scenario.h"

int
cardiac_parse(const char* elem, const char* value, struct cardiac* card)
{
	int sts = 0;

	if ((!elem) || (!value) || (!card))
	{
		return (-11);
	}
	if (strcmp(elem, ("rhythm")) == 0)
	{
		sprintf_s(card->rhythm, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, ("vpc")) == 0)
	{
		sprintf_s(card->vpc, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, ("pea")) == 0)
	{
		card->pea = atoi(value);
	}
	else if (strcmp(elem, ("vpc_freq")) == 0)
	{
		card->vpc_freq = atoi(value);
	}
	else if (strcmp(elem, ("vpc_delay")) == 0)
	{
		card->vpc_delay = atoi(value);
	}
	else if (strcmp(elem, ("vfib_amplitude")) == 0)
	{
		sprintf_s(card->vfib_amplitude, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, ("pwave")) == 0)
	{
		sprintf_s(card->pwave, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, ("rate")) == 0)
	{
		card->rate = atoi(value);
	}
	else if (strcmp(elem, ("transfer_time")) == 0)
	{
		card->transfer_time = atoi(value);
	}
	else if (strcmp(elem, ("pr_interval")) == 0)
	{
		card->pr_interval = atoi(value);
	}
	else if (strcmp(elem, ("qrs_interval")) == 0)
	{
		card->qrs_interval = atoi(value);
	}
	else if (strcmp(elem, ("bps_sys")) == 0)
	{
		card->bps_sys = atoi(value);
	}
	else if (strcmp(elem, ("bps_dia")) == 0)
	{
		card->bps_dia = atoi(value);
	}
	else if (strcmp(elem, ("nibp_rate")) == 0)
	{
		card->nibp_rate = atoi(value);
	}
	else if (strcmp(elem, "nibp_read") == 0)
	{
		card->nibp_read = atoi(value);
	}
	else if (strcmp(elem, "nibp_linked_hr") == 0)
	{
		card->nibp_linked_hr = atoi(value);
	}
	else if (strcmp(elem, "nibp_freq") == 0)
	{
		card->nibp_freq = atoi(value);
	}
	else if (strcmp(elem, ("ecg_indicator")) == 0)
	{
		card->ecg_indicator = atoi(value);
	}
	else if (strcmp(elem, ("bp_cuff")) == 0)
	{
		card->bp_cuff = atoi(value);
	}
	else if (strcmp(elem, ("heart_sound")) == 0)
	{
		sprintf_s(card->heart_sound, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, ("heart_sound_volume")) == 0)
	{
		card->heart_sound_volume = atoi(value);
	}
	else if (strcmp(elem, ("heart_sound_mute")) == 0)
	{
		card->heart_sound_mute = atoi(value);
	}
	else if (strcmp(elem, ("right_dorsal_pulse_strength")) == 0)
	{
		if ((strcmp(value, "none")) == 0)
		{
			card->right_dorsal_pulse_strength = 0;
		}
		else if ((strcmp(value, "weak")) == 0)
		{
			card->right_dorsal_pulse_strength = 1;
		}
		else if ((strcmp(value, "medium")) == 0)
		{
			card->right_dorsal_pulse_strength = 2;
		}
		else if ((strcmp(value, "strong")) == 0)
		{
			card->right_dorsal_pulse_strength = 3;
		}
		else
		{
			sts = 3;
		}
	}
	else if (strcmp(elem, ("left_dorsal_pulse_strength")) == 0)
	{
		if ((strcmp(value, "none")) == 0)
		{
			card->left_dorsal_pulse_strength = 0;
		}
		else if ((strcmp(value, "weak")) == 0)
		{
			card->left_dorsal_pulse_strength = 1;
		}
		else if ((strcmp(value, "medium")) == 0)
		{
			card->left_dorsal_pulse_strength = 2;
		}
		else if ((strcmp(value, "strong")) == 0)
		{
			card->left_dorsal_pulse_strength = 3;
		}
		else
		{
			sts = 3;
		}
	}
	else if (strcmp(elem, ("right_femoral_pulse_strength")) == 0)
	{
		if ((strcmp(value, "none")) == 0)
		{
			card->right_femoral_pulse_strength = 0;
		}
		else if ((strcmp(value, "weak")) == 0)
		{
			card->right_femoral_pulse_strength = 1;
		}
		else if ((strcmp(value, "medium")) == 0)
		{
			card->right_femoral_pulse_strength = 2;
		}
		else if ((strcmp(value, "strong")) == 0)
		{
			card->right_femoral_pulse_strength = 3;
		}
		else
		{
			sts = 3;
		}
	}
	else if (strcmp(elem, ("left_femoral_pulse_strength")) == 0)
	{
		if ((strcmp(value, "none")) == 0)
		{
			card->left_femoral_pulse_strength = 0;
		}
		else if ((strcmp(value, "weak")) == 0)
		{
			card->left_femoral_pulse_strength = 1;
		}
		else if ((strcmp(value, "medium")) == 0)
		{
			card->left_femoral_pulse_strength = 2;
		}
		else if ((strcmp(value, "strong")) == 0)
		{
			card->left_femoral_pulse_strength = 3;
		}
		else
		{
			sts = 3;
		}
	}
	else if (strcmp(elem, ("arrest")) == 0)
	{
		card->arrest = atoi(value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}

int
respiration_parse(const char* elem, const char* value, struct respiration* resp)
{
	int sts = 0;

	if ((!elem) || (!value) || (!resp))
	{
		return (-12);
	}
	if (strcmp(elem, "left_lung_sound") == 0)
	{
		sprintf_s(resp->left_lung_sound, STR_SIZE, "%s", value);
	}
	else if (strcmp(elem, "right_lung_sound") == 0)
	{
		sprintf_s(resp->right_lung_sound, STR_SIZE, "%s", value);
	}
	/* These are set by sim-mgr, not instructor
	else if ( strcmp(elem, "inhalation_duration" ) == 0 )
	{
		resp->inhalation_duration = atoi(value );
	}
	else if ( strcmp(elem, "exhalation_duration" ) == 0 )
	{
		resp->exhalation_duration = atoi(value );
	}
	*/
	else if (strcmp(elem, "left_lung_sound_volume") == 0)
	{
		resp->left_lung_sound_volume = atoi(value);
	}
	else if (strcmp(elem, "left_lung_sound_mute") == 0)
	{
		resp->left_lung_sound_mute = atoi(value);
	}
	else if (strcmp(elem, "right_lung_sound_volume") == 0)
	{
		resp->right_lung_sound_volume = atoi(value);
	}
	else if (strcmp(elem, "right_lung_sound_volume") == 0)
	{
		resp->right_lung_sound_volume = atoi(value);
	}
	else if (strcmp(elem, "right_lung_sound_mute") == 0)
	{
		resp->right_lung_sound_mute = atoi(value);
	}
	else if (strcmp(elem, "rate") == 0)
	{
		resp->rate = atoi(value);
	}
	else if (strcmp(elem, "spo2") == 0)
	{
		resp->spo2 = atoi(value);
	}
	else if (strcmp(elem, "etco2") == 0)
	{
		resp->etco2 = atoi(value);
	}
	else if (strcmp(elem, "transfer_time") == 0)
	{
		resp->transfer_time = atoi(value);
	}
	else if (strcmp(elem, "etco2_indicator") == 0)
	{
		resp->etco2_indicator = atoi(value);
	}
	else if (strcmp(elem, "spo2_indicator") == 0)
	{
		resp->spo2_indicator = atoi(value);
	}
	else if (strcmp(elem, "chest_movement") == 0)
	{
		resp->chest_movement = atoi(value);
	}
	else if (strcmp(elem, "manual_count") == 0)
	{
		resp->manual_count = atoi(value);
	}
	else if (strcmp(elem, "manual_breath") == 0)
	{
		char buf[512];
		sprintf_s(buf, 512, "%s %s %s", "manual_breath", elem, value);
		log_message("", buf);
		resp->manual_breath = atoi(value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}
int
getVidIndex(const char* value)
{
	if (isdigit((int)value[0]))
	{
		return (atoi(value));
	}
	else if (strncmp("vid", value, 3) == 0)
	{
		return (atoi(&value[3]));
	}
	return (-1);
}

int
telesim_parse(const char* elem, const char* value, struct telesim* ts)
{
	int sts = 0;
	int index;
	const char* ptr;
	const char* arg;

	//char buf[512];
	//sprintf_s(buf, 512, "%s %s %s", "telesim", elem, value);
	//log_message("", buf);

	if ((!elem) || (!value) || (!ts))
	{
		return (-13);
	}

	if (strncmp(elem, "enable", 6) == 0)
	{
		ts->enable = atoi(value);
	}
	else if (strncmp(elem, "name", 4) == 0)
	{
		index = getVidIndex(value);
		ptr = strchr(value, ':');
		if (index < 0)
		{
			sts = 1;
		}
		else if (index > TSIM_WINDOWS)
		{
			sts = 1;
		}
		else if (ptr[0] != ':')
		{
			sts = 1;
		}
		else
		{
			arg = &ptr[1];
			snprintf(ts->vid[index].name, STR_SIZE, "%s", arg);
		}
	}
	else if (strncmp(elem, "command", 7) == 0)
	{
		index = getVidIndex(value);
		ptr = strchr(value, ':');
		if (index < 0)
		{
			sts = 1;
		}
		else if (index > TSIM_WINDOWS)
		{
			sts = 1;
		}
		else if (ptr[0] != ':')
		{
			sts = 1;
		}
		else
		{
			arg = &ptr[1];
			ts->vid[index].command = atoi(arg);
			//ts->vid[index].next = rand();
		}
	}
	else if (strncmp(elem, "param", 5) == 0)
	{
		index = getVidIndex(value);
		ptr = strchr(value, ':');
		if (index < 0)
		{
			sts = 1;
		}
		else if (index > TSIM_WINDOWS)
		{
			sts = 1;
		}
		else if (ptr[0] != ':')
		{
			sts = 1;
		}
		else
		{
			arg = &ptr[1];
			ts->vid[index].param = atof(arg);
		}
	}
	else if (strncmp(elem, "next", 4) == 0)
	{
		index = getVidIndex(value);
		ptr = strchr(value, ':');
		if (index < 0)
		{
			sts = 1;
		}
		else if (index > TSIM_WINDOWS)
		{
			sts = 1;
		}
		else if (ptr[0] != ':')
		{
			sts = 1;
		}
		else
		{
			arg = &ptr[1];
			ts->vid[index].next = atoi(arg);
		}
	}
	else
	{
	}
	return (sts);
}
int
general_parse(const char* elem, const char* value, struct general* gen)
{
	int sts = 0;

	if ((!elem) || (!value) || (!gen))
	{
		return (-13);
	}
	if (strcmp(elem, "temperature_enable") == 0)
	{
		gen->temperature_enable = atoi(value);
	}
	else if (strcmp(elem, "temperature_units") == 0)
	{
		if (value[0] == 'F' || value[0] == 'f')
		{
			gen->temperature_units[0] = 'F';
			gen->temperature_units[1] = 0;
		}
		else if ( value[0] == 'C' || value[0] == 'c')
		{
			gen->temperature_units[0] = 'C';
			gen->temperature_units[1] = 0;
		}
 	}
	else if (strcmp(elem, "temperature") == 0)
	{
		gen->temperature = atoi(value);
	}
	else if (strcmp(elem, "transfer_time") == 0)
	{
		gen->transfer_time = atoi(value);
	}
	else if (strcmp(elem, "clock_start") == 0)
	{
		sprintf_s(gen->clockStart, 64, "%s", value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}

int
vocals_parse(const char* elem, const char* value, struct vocals* voc)
{
	int sts = 0;

	if ((!elem) || (!value) || (!voc))
	{
		return (-14);
	}
	if (strcmp(elem, "filename") == 0)
	{
		sprintf_s(voc->filename, FILENAME_SIZE, "%s", value);
	}
	else if (strcmp(elem, "repeat") == 0)
	{
		voc->repeat = atoi(value);
	}
	else if (strcmp(elem, "volume") == 0)
	{
		voc->volume = atoi(value);
	}
	else if (strcmp(elem, "play") == 0)
	{
		voc->play = atoi(value);
	}
	else if (strcmp(elem, "mute") == 0)
	{
		voc->mute = atoi(value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}
int
media_parse(const char* elem, const char* value, struct media* med)
{
	int sts = 0;

	if ((!elem) || (!value) || (!med))
	{
		return (-14);
	}
	if (strcmp(elem, "filename") == 0)
	{
		sprintf_s(med->filename, FILENAME_SIZE, "%s", value);
	}
	else if (strcmp(elem, "play") == 0)
	{
		med->play = atoi(value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}
int
cpr_parse(const char* elem, const char* value, struct cpr* cpr)
{
	int sts = 0;

	if ((!elem) || (!value) || (!cpr))
	{
		return (-15);
	}
	if (strcmp(elem, "duration") == 0)
	{
		cpr->duration = atoi(value);
	}
	else if (strcmp(elem, "compression") == 0)
	{
		cpr->compression = atoi(value);
	}
	else
	{
		sts = 1;
	}
	return (sts);
}
/**
* initializeParameterStruct
* @initParams: Pointer to a "struct instructor"
*
* Initialize the parameters structure to be "no set" on all parameters.
* For strings, they are null. For numerics, they are -1.
*/

void
initializeParameterStruct(struct instructor* initParams)
{
	memset(initParams, 0, sizeof(struct instructor));

	initParams->cardiac.vpc_freq = -1;
	initParams->cardiac.vpc_delay = -1;
	initParams->cardiac.pea = -1;
	initParams->cardiac.rate = -1;
	initParams->cardiac.nibp_rate = -1;
	initParams->cardiac.nibp_read = -1;
	initParams->cardiac.nibp_linked_hr = -1;
	initParams->cardiac.nibp_freq = -1;
	initParams->cardiac.pr_interval = -1;
	initParams->cardiac.qrs_interval = -1;
	initParams->cardiac.bps_sys = -1;
	initParams->cardiac.bps_dia = -1;
	initParams->cardiac.right_dorsal_pulse_strength = -1;
	initParams->cardiac.right_femoral_pulse_strength = -1;
	initParams->cardiac.left_dorsal_pulse_strength = -1;
	initParams->cardiac.left_femoral_pulse_strength = -1;
	initParams->cardiac.heart_sound_volume = -1;
	initParams->cardiac.heart_sound_mute = -1;
	initParams->cardiac.ecg_indicator = -1;
	initParams->cardiac.bp_cuff = -1;
	initParams->cardiac.transfer_time = -1;
	initParams->cardiac.arrest = -1;

	initParams->respiration.inhalation_duration = -1;
	initParams->respiration.exhalation_duration = -1;
	initParams->respiration.left_lung_sound_volume = -1;
	initParams->respiration.left_lung_sound_mute = -1;
	initParams->respiration.right_lung_sound_volume = -1;
	initParams->respiration.right_lung_sound_mute = -1;
	initParams->respiration.spo2 = -1;
	initParams->respiration.rate = -1;
	initParams->respiration.etco2 = -1;
	initParams->respiration.etco2_indicator = -1;
	initParams->respiration.spo2_indicator = -1;
	initParams->respiration.chest_movement = -1;
	initParams->respiration.transfer_time = -1;
	initParams->respiration.manual_breath = -1;
	initParams->respiration.manual_count = -1;

	initParams->general.temperature = -1;
	initParams->general.temperature_enable = -1;
	initParams->general.transfer_time = -1;

	initParams->vocals.repeat = -1;
	initParams->vocals.volume = -1;
	initParams->vocals.play = -1;
	initParams->vocals.mute = -1;

	initParams->media.play = -1;

	initParams->scenario.record = -1;

	initParams->cpr.compression = -1;
	initParams->cpr.duration = -1;
	initParams->cpr.release = -1;
	initParams->cpr.last = -1;

	initParams->telesim.enable = -1;
	initParams->telesim.vid[0].name[0] = 0;
	initParams->telesim.vid[0].command = -1;
	initParams->telesim.vid[0].param = -1;
	initParams->telesim.vid[0].next = -1;
	initParams->telesim.vid[1].name[0] = 0;
	initParams->telesim.vid[1].command = -1;
	initParams->telesim.vid[1].param = -1;
	initParams->telesim.vid[1].next = -1;
}

/**
* processInit
* @initParams: Pointer to a "struct instructor"
*
* Transfer the instructions from the initParams to the instructor portion of
* the shared data space, to activate all the controls.
*/
void
processInit(struct instructor* initParams)
{
	takeInstructorLock();

	// Copy initParams to shared instructor interface (not the sema or scenario sections)

	memcpy(&simmgr_shm->instructor.cardiac, &initParams->cardiac, sizeof(struct cardiac));
	memcpy(&simmgr_shm->instructor.respiration, &initParams->respiration, sizeof(struct respiration));
	memcpy(&simmgr_shm->instructor.general, &initParams->general, sizeof(struct general));
	memcpy(&simmgr_shm->instructor.vocals, &initParams->vocals, sizeof(struct vocals));
	memcpy(&simmgr_shm->instructor.media, &initParams->media, sizeof(struct media));
	memcpy(&simmgr_shm->instructor.cpr, &initParams->cpr, sizeof(struct cpr));

	if (strlen(initParams->telesim.vid[0].name) > 0 ||
		initParams->telesim.vid[0].command != -1 ||
		initParams->telesim.vid[0].param != -1)
	{
		initParams->telesim.vid[0].next = rand();
	}
	if (strlen(initParams->telesim.vid[1].name) > 0 ||
		initParams->telesim.vid[1].command != -1 ||
		initParams->telesim.vid[1].param != -1)
	{
		initParams->telesim.vid[1].next = rand();
	}
	memcpy(&simmgr_shm->instructor.telesim, &initParams->telesim, sizeof(struct telesim));
	releaseInstructorLock();

	// Delay to allow simmgr to pick up the changes
	Sleep(500);

}
int
getValueFromName(char* param_class, char* param_element)
{
	int rval = -1;

	if (strcmp(param_class, "cardiac") == 0)
	{
		if (strcmp(param_element, "vpc_freq") == 0)
			rval = simmgr_shm->status.cardiac.vpc_freq;
		else if (strcmp(param_element, "vpc_delay") == 0)
			rval = simmgr_shm->status.cardiac.vpc_delay;
		else if (strcmp(param_element, "pea") == 0)
			rval = simmgr_shm->status.cardiac.pea;
		else if (strcmp(param_element, "rate") == 0)
			rval = simmgr_shm->status.cardiac.rate;
		else if (strcmp(param_element, "nibp_rate") == 0)
			rval = simmgr_shm->status.cardiac.nibp_rate;
		else if (strcmp(param_element, "nibp_read") == 0)
			rval = simmgr_shm->status.cardiac.nibp_read;
		else if (strcmp(param_element, "nibp_linked_hr") == 0)
			rval = simmgr_shm->status.cardiac.nibp_linked_hr;
		else if (strcmp(param_element, "nibp_freq") == 0)
			rval = simmgr_shm->status.cardiac.nibp_freq;
		else if (strcmp(param_element, "pr_interval") == 0)
			rval = simmgr_shm->status.cardiac.pr_interval;
		else if (strcmp(param_element, "qrs_interval") == 0)
			rval = simmgr_shm->status.cardiac.qrs_interval;
		else if (strcmp(param_element, "bps_sys") == 0)
			rval = simmgr_shm->status.cardiac.bps_sys;
		else if (strcmp(param_element, "bps_dia") == 0)
			rval = simmgr_shm->status.cardiac.bps_dia;
		else if (strcmp(param_element, "ecg_indicator") == 0)
			rval = simmgr_shm->status.cardiac.ecg_indicator;
		else if (strcmp(param_element, "bp_cuff") == 0)
			rval = simmgr_shm->status.cardiac.bp_cuff;
		else if (strcmp(param_element, "cpr_time") == 0)
			rval = simmgr_shm->status.cardiac.bp_cuff;
		else if (strcmp(param_element, "arrest") == 0)
			rval = simmgr_shm->status.cardiac.arrest;
	}
	else if (strcmp(param_class, "respiration") == 0)
	{
		if (strcmp(param_element, "spo2") == 0)
			rval = simmgr_shm->status.respiration.spo2;
		else if (strcmp(param_element, "rate") == 0)
			rval = simmgr_shm->status.respiration.rate;
		else if (strcmp(param_element, "etco2_indicator") == 0)
			rval = simmgr_shm->status.respiration.etco2_indicator;
		else if (strcmp(param_element, "spo2_indicator") == 0)
			rval = simmgr_shm->status.respiration.spo2_indicator;
		else if (strcmp(param_element, "chest_movement") == 0)
			rval = simmgr_shm->status.respiration.chest_movement;
		else if (strcmp(param_element, "manual_count") == 0)
			rval = simmgr_shm->status.respiration.manual_count;
		else if (strcmp(param_element, "etco2") == 0)
			rval = simmgr_shm->status.respiration.etco2;
	}
	else if (strcmp(param_class, "general") == 0)
	{
		if (strcmp(param_element, "temperature_enable") == 0)
			rval = simmgr_shm->status.general.temperature_enable;
		else if (strcmp(param_element, "temperature") == 0)
			rval = simmgr_shm->status.general.temperature;
	}
	else if (strcmp(param_class, "telesim") == 0)
	{
		if (strcmp(param_element, "enable") == 0)
			rval = simmgr_shm->status.telesim.enable;
	}
	else if (strcmp(param_class, "cpr") == 0)
	{
		if (strcmp(param_element, "duration") == 0)
			rval = simmgr_shm->status.cpr.duration;
	}
	else if (strcmp(param_class, "pulse") == 0)
	{
		if (strcmp(param_element, "left_femoral") == 0)
			rval = simmgr_shm->status.pulse.left_femoral;
		if (strcmp(param_element, "right_femoral") == 0)
			rval = simmgr_shm->status.pulse.right_femoral;
		if (strcmp(param_element, "duration") == 0)
			rval = simmgr_shm->status.pulse.duration;
		if (strcmp(param_element, "active") == 0)
			rval = simmgr_shm->status.pulse.active;
	}
	return (rval);
}
