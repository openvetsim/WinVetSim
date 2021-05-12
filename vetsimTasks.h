#pragma once

// Windows Header Files
#include <iostream>
#include <thread>
#include <functional>


int isServerRunning(void);
int startPHPServer(void);
void stopPHPServer(void);
void simstatusMain(void);

void start_task(const char* name, std::function<void(void)> func);
void timer_start(std::function<void(void)> func, unsigned int interval);

