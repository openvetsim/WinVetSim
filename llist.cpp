/*
 * sim-llist.c
 *
 * Link List functions
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

#include <stdio.h>
#include <stdlib.h>
#include "llist.h"

void
insert_llist(struct snode* entry, struct snode* list)
{
	int limit = 100;

	// Scan to the first NULL entry (end of list)
	while (list->next)
	{
		list = list->next;
		if (limit-- == 0)
		{
			fprintf(stderr, "insert_llist: Limit exceeded\n");
			exit(-1);
		}
	}
	list->next = entry;
}

struct snode*
	get_next_llist(struct snode* entry)
{
	return (entry->next);
}