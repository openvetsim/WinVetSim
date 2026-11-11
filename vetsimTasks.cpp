/*
 * vetsimTasksim.cpp
 *
 * SimMgr applicatiopn
 *
 * This file is part of the WinVetSim distribution.
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

#include "vetsimTasks.h"
#include <thread>
#include <ostream>
#include <mutex>

using namespace std;

// Serializes the "Task Started" messages so concurrent start_task calls don't interleave on cout.
static std::mutex startTaskOutputLock;

// Start a task to run once. Might run forever.
std::thread::id start_task(const char* name, std::function<void(void)> func)
{
	std::thread::id id;

	std::thread proc = std::thread([func]() {	func(); });
	id = proc.get_id();		// Must be read before detach(); afterwards it is always empty.
	proc.detach();
	{
		std::lock_guard<std::mutex> guard(startTaskOutputLock);
		cout << "Task Started: " << name << " " << id << endl;
	}
	return (id);
}

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
