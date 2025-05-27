/*
 * simstatus.cpp
 *
 * Provide status/control operations.
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

#include "vetsim.h"
#include "cgiClass.h"
#include <map>
#include <unordered_map>
#include <utility>

using namespace std;

void sendStatus(void);
void sendQuickStatus(void);
void sendSimctrData(void);
void replaceAll(char* args, size_t len, const char* needle, const char replace);

string htmlReply;
int closeFlag = 0;

void makejson(string key, string content)
{
	htmlReply += "\"" + key + "\":\"" + content + "\"";
}
void makejson(string key, char *content)
{
	htmlReply += "\"" + key + "\":\"" + content + "\"";
}
void makejson(string key, int content)
{
	auto buf = to_string(content);

	htmlReply += "\"" + key + "\":\"" + buf + "\"";
}
std::vector<std::string> explode(std::string const& s, char delim)
{
	std::vector<std::string> result;
	std::istringstream iss(s);

	for (std::string token; std::getline(iss, token, delim); )
	{
		result.push_back(token);
	}

	return result;
}

void
releaseLock(int taken)
{
	if (taken)
	{
		releaseInstructorLock();
	}
}

int debug = 0;

#define BUF_SIZE	2048
char smbuf[BUF_SIZE];		// Used for logging messages

#define DEFAULT_BUFLEN 4096

char recvbuf[DEFAULT_BUFLEN];
char firstLine[DEFAULT_BUFLEN];
int simstatusHandleCommand(char *args);
void sendNotFound(char* path);

void
simstatusMain(void)
{
	int portno = PORT_STATUS;
	int i;
	int error;
	char* sesid = NULL;
	SOCKET sfd;
	SOCKET cfd;
	struct sockaddr client_addr;
	int socklen;
	WSADATA w;

	printf("simstatus is on port %d\n", portno);

	SOCKADDR_IN addr;                     // The address structure for a TCP socket

	int iResult, iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;

	error = WSAStartup(0x0202, &w);  // Fill in WSA info
	if (error)
	{
		cout << "WSAStartup fails: " << GetLastErrorAsString();
		return;                     //For some reason we couldn't start Winsock
	}

	addr.sin_family = AF_INET;            // Address family
	addr.sin_port = htons(portno);       // Assign port to this socket

	//Accept a connection from any IP using INADDR_ANY
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create socket

	if (sfd == INVALID_SOCKET)
	{
		cout << "socket(): INVALID_SOCKET " << GetLastErrorAsString();
		return;                     //Don't continue if we couldn't create a //socket!!
	}

	if (::bind(sfd, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		//We couldn't bind (this will happen if you try to bind to the same  
		//socket more than once)
		cout << "bind(): SOCKET_ERROR " << GetLastErrorAsString();
		return;
	}
	if (listen(sfd, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(sfd);
		WSACleanup();
		return;
	}
	socklen = sizeof(struct sockaddr_in);

	while (1)
	{
		cfd = accept(sfd, (struct sockaddr*)&client_addr, &socklen);
		if (cfd >= 0)
		{
			htmlReply.clear();

			// Receive HTTP Header
			ZeroMemory(recvbuf, recvbuflen);
			iResult = recv(cfd, recvbuf, recvbuflen, 0);
			if (iResult > 0) 
			{
				//printf("Bytes received: %d\n", iResult);
				//cout << "START-------\n" << recvbuf << "\n---------END\n" << endl;
				// Copy the first line by looking for first newline
				for (i = 0; recvbuf[i]; i++)
				{
					if (i < sizeof(recvbuf))
					{
						if (recvbuf[i] == '\n' || recvbuf[i] == '\r')
							break;
						firstLine[i] = recvbuf[i];
					}
					else
					{
						break;
					}
				}
				firstLine[i] = 0;

				//if (firstLine) cout << "FirstLine: " << firstLine << endl;
				int method = 0;
				// Determine Method
				if (strncmp(firstLine, "GET", 3) == 0)
				{
					//cout << "Method:    GET" << endl;
					method = 1;
				}
				else if (strncmp(firstLine, "POST", 4) == 0)
				{
					//cout << "Method:    POST" << endl;
					method = 2;
				}
				else
				{
					//cout << "Method:    unknown" << endl;
					method = 0;
				}
				char* cptr = NULL;
				char* path = NULL;
				char* args = NULL;
				if (method == 1)
				{
					cptr = strchr(firstLine, '?');
					if (cptr != NULL)
					{
						args = cptr + 1;
						*cptr = 0;
					}
				}
				else if (method == 2)
				{
					// Args are in the last line of the Request header
					
					for ( i = iResult ; i >= 0 ; i-- )
					{
						if (i < sizeof(recvbuf))
						{
							if (recvbuf[i] == '\n' || recvbuf[i] == '\r')
							{
								args = &recvbuf[i + 1];
								break;
							}
						}
						else
						{
							break;
						}
					}
				}
				if (args)
				{
					replaceAll(args, strlen(args), "%3A", ':');
					replaceAll(args, strlen(args), "+", ' ');
					replaceAll(args, strlen(args), "%20", ' ');
					replaceAll(args, strlen(args), "%2B", '+');
					cptr = strstr(args, " HTTP/");
					if (cptr)
					{
						cptr[0] = 0;
					}
				}
				cptr = strchr(firstLine, '/');
				if (cptr == NULL)
				{
					path = NULL;
				}
				else
				{
					path = cptr + 1;
					*cptr = 0;
					cptr = strchr(path, ' ');
					if (cptr)
					{
						*cptr = 0;
					}
				}
				
				if (path)
				{
					//cout << "Path:      " << path << endl;
				}
				if (args)
				{

					//cout << "Args:      " << args << endl;
				}
				//cout << "------------" << endl;
				if (path)
				{
					if (strcmp(path, "simstatus.cgi") == 0)
					{
						simstatusHandleCommand(args );
					}
					else if (strcmp(path, "cgi-bin/simstatus.cgi") == 0)
					{
						simstatusHandleCommand(args );
					}
					else
					{
						sendNotFound(path);
					}
				}
				// Send reply
				//cout << "Returning\n------------\n" << htmlReply << "\n------------\n" << endl;
				iSendResult = send(cfd,  htmlReply.c_str(), (int)htmlReply.length(),  0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed: %d\n", WSAGetLastError());
				}
				else
				{
					//printf("Bytes sent: %d\n", iSendResult);
				}
			}
			else if (iResult == 0)
			{
				//printf("Connection closing...\n");
			}
			else 
			{
				printf("recv failed: %d\n", WSAGetLastError());
			}
			closesocket(cfd);
		}
		if (closeFlag)
		{
			// SHutdown the PHP Server
			stopPHPServer();
#ifdef DEBUG
			printf("Close Window to Exit\n");
			while (1)
			{
				Sleep(10);
			}
#else
			// Close the application
			ExitProcess(0);
#endif
			
		}
	}
	exit(203);
}
/*
Parse Samples:

GET /status.cgi?status=1

GET /cgi-bin/simstatus.cgi?set:cardiac:rhythm=afib&set:cardiac:vpc=none&set:cardiac:pea=0&set:cardiac:arrest=0&set:cardiac:vpc_freq=10&set:cardiac:vfib_amplitude=high&set:cardiac:rate=80 HTTP/1.1
Host: 192.168.1.22
Connection: keep-alive
Accept: application/json, text/javascript, *; q = 0.01
User - Agent: Mozilla / 5.0 (Windows NT 10.0; Win64; x64) AppleWebKit / 537.36 (KHTML, like Gecko) Chrome / 89.0.4389.82 Safari / 537.36
X - Requested - With : XMLHttpRequest
Referer : http://192.168.1.22/sim-ii/ii.php
Accept - Encoding : gzip, deflate
Accept - Language : en - US, en; q = 0.9, es - MX; q = 0.8, es; q = 0.7, ca; q = 0.6, mg; q = 0.5
Cookie: userID = 6; PHPSESSID = jtha8dsa284hes9ehtlvj3bpnh

*/
void
sendNotFound(char *path)
{
	string str(path);

	htmlReply += "HTTP/1.1 404 Not Found\r\n";
	htmlReply += "Access-Control-Allow-Origin: *\r\n";
	htmlReply += "Server:vetsim / 1.0\r\n";
	htmlReply += "Content-Type:  text\r\n";
	htmlReply += "Connection: close\r\n\r\n";
	htmlReply += "< !doctype html > <html><head> < title>404 Not Found< / title><style>\n";
	htmlReply += "body{ background - color: #cfcfcf; color: #333333; margin : 0; padding : 0; }\n";
	htmlReply += "h1{ font - size: 1.5em; font - weight: normal; background - color: #9999cc; min - height:2em; line - height:2em; border - bottom: 1px inset black; margin : 0; }\n";
	htmlReply += "h1, p{ padding - left: 10px; }\n";
	htmlReply += "code.url{ background - color: #eeeeee; font - family:monospace; padding:0 2px; }\n";
	htmlReply += "< / style>\n";
	htmlReply += "< / head><body><h1>Not Found< / h1><p>The requested resource <code class = 'url'> / " + str + "< / code> was not found on this server.< / p>< / body> < / html>\n";
}
struct argument
{
	string key;
	string value;
};

char defaultArgs[] = "status=1";

int
simstatusHandleCommand(char *args)
{
	char buffer[MSG_LENGTH];
	char cmd[32];
	int i;
	int set_count = 0;
	int sts;
	char sesid[512] = { 0, };
	int userid = -1;
	argument arg;
	int ss_iiLockTaken = 0;

	std::string key;
	std::string value;
	std::string command;
	std::string theTime;
	std::vector<std::string> v;

	map<int, argument> argList;
	if (strlen(args) == 0)
	{
		args = defaultArgs;
	}
	char* keyP = &args[0];
	char* valP = strchr(args, '=');
	char* nextP = strchr(args, '&');
	i = 0;
	while (valP)
	{
		valP[0] = 0;
		valP += 1;
		if (nextP)
		{
			nextP[0] = 0;
			nextP += 1;
		}
		arg.key = keyP;
		arg.value = valP;
		argList.insert(pair<int,argument>(i, arg));
		if (nextP != NULL)
		{
			keyP = nextP;
			valP = strchr(nextP, '=');
			nextP = strchr(nextP, '&');
		}
		else
		{
			valP = NULL;
		}
		i++;
	}
	htmlReply += "HTTP/1.1 200 OK\r\n";
	htmlReply += "Server:vetsim / 1.0\r\n";
	htmlReply += "Access-Control-Allow-Origin: *\r\n";
	htmlReply += "Content-Type:  application/json\r\n";
	htmlReply += "Connection: close\r\n\r\n";

	htmlReply += "{\n";
	map<int, argument>::iterator itr;
	//cout << "\tKey\tValue\n";

	//for (itr = argList.begin(); itr != argList.end(); ++itr) {
	//	arg = itr->second;
		//cout << '\t' << arg.key	<< '\t' << arg.value << '\n';
	//	makejson(arg.key, arg.value);
	//	htmlReply += ",\n";
	//}
	//cout << endl;

	//get_date(buffer, sizeof(buffer));
	//sprintf_s(buffer, sizeof(buffer), "Jan 01, 2001");
	//makejson("date", buffer);
	//htmlReply += ",\n    ";

	for (itr = argList.begin(); itr != argList.end(); ++itr) 
	{
		arg = itr->second;
		key = arg.key;
		value = arg.value;
		if (key.compare("PHPSESSID") == 0)
		{
			sprintf_s(sesid, sizeof(sesid), "%s", value.c_str());
			makejson(key, value);
			htmlReply += ",\n";
		}
		else if (key.compare("simIIUserID") == 0)
		{
			userid = atoi(value.c_str());
			makejson(key, value);
			htmlReply += ",\n";
		}
		else if (key.compare("userID") == 0)
		{
			userid = atoi(value.c_str());
			makejson(key, value);
			htmlReply += ",\n";
		}
		else if (key.compare("close") == 0)
		{
			i = atoi(value.c_str());
			if (i == 565)
			{
				makejson(key, value);
				htmlReply += ",\n";
				closeFlag = 1;
			}
			else
			{
				makejson("error", "bad param");
				htmlReply += ",\n";
			}
		}
	}

	sprintf_s(cmd, sizeof(cmd), "none");
	// If any "set" commands are in the GET/POST list, then take the Instructor Interface lock
	for (itr = argList.begin(); itr != argList.end(); ++itr)
	{
		arg = itr->second;
		key = arg.key;
		value = arg.value;

		if (key.compare(0, 4, "set:") == 0)
		{
			if (takeInstructorLock() != 0)
			{
				makejson("status", "Fail");
				htmlReply += ",\n    ";
				makejson("error", "Could not get Instructor Mutex");
				htmlReply += "\n}\n";
				return (0);
			}

			ss_iiLockTaken = 1;
			break;
		}
	}

	i = 0;
	// Parse the submitted GET/POST elements
	for (itr = argList.begin(); itr != argList.end(); ++itr)
	{
		arg = itr->second;
		key = arg.key;
		value = arg.value;
		//printf("Key \"%s\" Value \"%s\"\n", key.c_str(), value.c_str());
		if (i > 0)
		{
			htmlReply += ",\n";
		}
		i++;
		if (key.compare("qstat") == 0)
		{
			// The Quick Status
			sendQuickStatus();
		}
		else if (key.compare("check") == 0)
		{
			makejson("check", "check is ok");
			htmlReply += ",\n";
			/*
			char* cp;
			cp = do_command_read("/usr/bin/uptime", buffer, sizeof(buffer) - 1);
			if (cp == NULL)
			{
				makejson("uptime", "no data");
			}
			else
			{
				makejson("uptime", buffer);
			}
			htmlReply += ",\n";
			*/
			makejson("ip_addr", simmgr_shm->server.ip_addr);
			htmlReply += ",\n";
			makejson("wifi_ip_addr", simmgr_shm->server.wifi_ip_addr);
			htmlReply += ",\n";
			makejson("port_pulse", PORT_PULSE);
			htmlReply += ",\n";
			makejson("port_status", PORT_STATUS);
		}
		/*
		else if (key.compare("uptime") == 0)
		{
			cp = do_command_read("/usr/bin/uptime", buffer, sizeof(buffer) - 1);
			if (cp == NULL)
			{
				makejson("uptime", "no data");
			}
			else
			{
				makejson("uptime", buffer);
			}
		}
		*/
		else if (key.compare("date") == 0)
		{
			get_date(buffer, sizeof(buffer));
			makejson("date", buffer);
			//htmlReply += ",\n";
			//makejson("date_t", simmgr_shm->server.server_time );
		}
		
		else if (key.compare("ip") == 0)
		{
			makejson("ip_addr", simmgr_shm->server.ip_addr);
		}
		else if (key.compare("host") == 0)
		{
			makejson("hostname", simmgr_shm->server.name);
		}
		else if (key.compare("time") == 0)
		{
			theTime = std::string(simmgr_shm->server.server_time);

			if (!theTime.empty() && theTime[theTime.length() - 1] == '\n')
			{
				theTime.erase(theTime.length() - 1);
			}
			makejson("time", theTime.c_str());
		}
		else if (key.compare("status") == 0)
		{
			// The meat of the task - Return the content of the SHM
			sendStatus();
		}
		else if (key.compare("simctrldata") == 0)
		{
			// Return abbreviated status for sim controller
			sendSimctrData();
		}
		else if (key.compare(0, 4, "set:") == 0)
		{
			set_count++;
			// set command: Split to segments to construct the reference
			v = explode(key, ':');
			sprintf_s(buffer, MSG_LENGTH, " \"set_%d\" : {\n    ", set_count);
			htmlReply += buffer;
			makejson("class", v[1]);
			htmlReply += ",\n    ";
			makejson("param", v[2]);
			htmlReply += ",\n    ";
			makejson("value", value);
			htmlReply += ",\n    ";
			sts = 0;

			if (v[1].compare("cardiac") == 0)
			{
				printf("Calling Cardiac Parse, \"%s\", \"%s\"\n", v[2].c_str(), value.c_str());
				sts = cardiac_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.cardiac);
			}
			else if (v[1].compare("scenario") == 0)
			{
				if (v[2].compare("active") == 0)
				{
					sprintf_s(simmgr_shm->instructor.scenario.active, STR_SIZE, "%s", value.c_str());
				}
				else if (v[2].compare("state") == 0)
				{
					sprintf_s(simmgr_shm->instructor.scenario.state, STR_SIZE, "%s", value.c_str());
				}
				else if (v[2].compare("record") == 0)
				{
					simmgr_shm->instructor.scenario.record = atoi(value.c_str());
				}
				else
				{
					sts = 1;
				}
			}
			else if (v[1].compare("respiration") == 0)
			{
				sts = respiration_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.respiration);
			}
			else if (v[1].compare("general") == 0)
			{
				sts = general_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.general);
			}
			else if (v[1].compare("telesim") == 0)
			{
				sts = telesim_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.telesim);
			}
			else if (v[1].compare("vocals") == 0)
			{
				sts = vocals_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.vocals);
			}
			else if (v[1].compare("media") == 0)
			{
				sts = media_parse(v[2].c_str(), value.c_str(), &simmgr_shm->instructor.media);
			}
			else if (v[1].compare("event") == 0)
			{
				if (v[2].compare("event_id") == 0)
				{
					if (value.length() != 0)
					{
						addEvent((char*)value.c_str());
						sts = 0;
					}
					else
					{
						sts = 4;
					}
				}
				else if (v[2].compare("comment") == 0)
				{
					if (value.length() != 0)
					{
						sprintf_s(smbuf, sizeof(smbuf), "Comment: %s", (char*)value.c_str());
						if (strcmp(simmgr_shm->status.scenario.state, "Running") == 0 ||
							strcmp(simmgr_shm->status.scenario.state, "Paused") == 0)
						{
							addComment(smbuf);
							sts = 0;
						}
						else
						{
							addComment(smbuf);
							sts = 5;
						}
					}
					else
					{
						sts = 4;
					}
				}
				else
				{
					sts = 2;
				}
			}
			else if (v[1].compare("cpr") == 0)
			{
				if (v[2].compare("compression") == 0)
				{
					simmgr_shm->instructor.cpr.compression = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("release") == 0)
				{
					simmgr_shm->instructor.cpr.release = atoi(value.c_str());
					sts = 0;
				}
				else
				{
					sts = 2;
				}
			}
			else if (v[1].compare("pulse") == 0)
			{
				if (v[2].compare("right_dorsal") == 0)
				{
					simmgr_shm->status.pulse.right_dorsal = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("left_dorsal") == 0)
				{
					simmgr_shm->status.pulse.left_dorsal = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("right_femoral") == 0)
				{
					simmgr_shm->status.pulse.right_femoral = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("left_femoral") == 0)
				{
					simmgr_shm->status.pulse.left_femoral = atoi(value.c_str());
					sts = 0;
				}
				else
				{
					sts = 2;
				}
			}
			else if (v[1].compare("auscultation") == 0)
			{
				if (v[2].compare("side") == 0)
				{
					simmgr_shm->status.auscultation.side = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("row") == 0)
				{
					simmgr_shm->status.auscultation.row = atoi(value.c_str());
					sts = 0;
				}
				else if (v[2].compare("col") == 0)
				{
					simmgr_shm->status.auscultation.col = atoi(value.c_str());
					sts = 0;
				}
				else
				{
					sts = 2;
				}
			}
			else
			{
				sts = 2;
			}
			if (sts == 1)
			{
				makejson("status", "invalid param");
			}
			else if (sts == 2)
			{
				makejson("status", "invalid class");
			}
			else if (sts == 3)
			{
				makejson("status", "invalid parameter");
			}
			else if (sts == 4)
			{
				makejson("status", "Null string in parameter");
			}
			else if (sts == 5)
			{
				makejson("status", "Scenario is not running");
			}
			else
			{
				makejson("status", "ok");
			}
			htmlReply += "\n    }";
		}
		else
		{
			makejson("Invalid Command", cmd);
		}
	}

	htmlReply += "\n}\n";
	if (ss_iiLockTaken)
	{
		releaseLock(ss_iiLockTaken);
	}
	
	ss_iiLockTaken = 0;
	return (0);
}


void
sendSimctrData(void)
{
	char buffer[256];


	htmlReply += " \"cardiac\" : {\n";
	makejson("vpc", simmgr_shm->status.cardiac.vpc);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pea, buffer, 256, 10);
	makejson("pea", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.vpc_freq, buffer, 256, 10);
	makejson("vpc_freq", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.vpc_delay, buffer, 256, 10);
	makejson("vpc_delay", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_ltoa_s(simmgr_shm->status.cardiac.avg_rate, buffer, 256, 10);
	makejson("avg_rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_rate, buffer, 256, 10);
	makejson("nibp_rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_read, buffer, 256, 10);
	makejson("nibp_read", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_linked_hr, buffer, 256, 10);
	makejson("nibp_linked_hr", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_freq, buffer, 256, 10);
	makejson("nibp_freq", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCount, buffer, 256, 10);
	makejson("pulseCount", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCountVpc, buffer, 256, 10);
	makejson("pulseCountVpc", buffer);
	htmlReply += ",\n";
	makejson("pwave", simmgr_shm->status.cardiac.pwave);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pr_interval, buffer, 256, 10);
	makejson("pr_interval", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.qrs_interval, buffer, 256, 10);
	makejson("qrs_interval", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.bps_sys, buffer, 256, 10);
	makejson("bps_sys", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.bps_dia, buffer, 256, 10);
	makejson("bps_dia", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.arrest, buffer, 256, 10);
	makejson("arrest", buffer);
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.right_dorsal_pulse_strength)
	{
	case 0:
		makejson("right_dorsal_pulse_strength", "none");
		break;
	case 1:
		makejson("right_dorsal_pulse_strength", "weak");
		break;
	case 2:
		makejson("right_dorsal_pulse_strength", "medium");
		break;
	case 3:
		makejson("right_dorsal_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.right_dorsal_pulse_strength, buffer, 256, 10);
		makejson("right_dorsal_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.left_dorsal_pulse_strength)
	{
	case 0:
		makejson("left_dorsal_pulse_strength", "none");
		break;
	case 1:
		makejson("left_dorsal_pulse_strength", "weak");
		break;
	case 2:
		makejson("left_dorsal_pulse_strength", "medium");
		break;
	case 3:
		makejson("left_dorsal_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.left_dorsal_pulse_strength, buffer, 256, 10);
		makejson("left_dorsal_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.right_femoral_pulse_strength)
	{
	case 0:
		makejson("right_femoral_pulse_strength", "none");
		break;
	case 1:
		makejson("right_femoral_pulse_strength", "weak");
		break;
	case 2:
		makejson("right_femoral_pulse_strength", "medium");
		break;
	case 3:
		makejson("right_femoral_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.right_femoral_pulse_strength, buffer, 256, 10);
		makejson("right_femoral_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.left_femoral_pulse_strength)
	{
	case 0:
		makejson("left_femoral_pulse_strength", "none");
		break;
	case 1:
		makejson("left_femoral_pulse_strength", "weak");
		break;
	case 2:
		makejson("left_femoral_pulse_strength", "medium");
		break;
	case 3:
		makejson("left_femoral_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.left_femoral_pulse_strength, buffer, 256, 10);
		makejson("left_femoral_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.heart_sound_volume, buffer, 256, 10);
	makejson("heart_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.heart_sound_mute, buffer, 256, 10);
	makejson("heart_sound_mute", buffer);
	htmlReply += ",\n";
	makejson("heart_sound", simmgr_shm->status.cardiac.heart_sound);
	htmlReply += ",\n";
	makejson("rhythm", simmgr_shm->status.cardiac.rhythm);
	htmlReply += "\n},\n";

	htmlReply += " \"defibrillation\" : {\n";
	_itoa_s(simmgr_shm->status.defibrillation.shock, buffer, 256, 10);
	makejson("shock", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"cpr\" : {\n";
	_itoa_s(simmgr_shm->status.cpr.running, buffer, 256, 10);
	makejson("running", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"respiration\" : {\n";
	makejson("left_lung_sound", simmgr_shm->status.respiration.left_lung_sound);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.left_lung_sound_volume, buffer, 256, 10);
	makejson("left_lung_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.left_lung_sound_mute, buffer, 256, 10);
	makejson("left_lung_sound_mute", buffer);
	htmlReply += ",\n";
	makejson("right_lung_sound", simmgr_shm->status.respiration.right_lung_sound);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.right_lung_sound_volume, buffer, 256, 10);
	makejson("right_lung_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.right_lung_sound_mute, buffer, 256, 10);
	makejson("right_lung_sound_mute", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.inhalation_duration, buffer, 256, 10);
	makejson("inhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.exhalation_duration, buffer, 256, 10);
	makejson("exhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.awRR, buffer, 256, 10);
	makejson("awRR", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.chest_movement, buffer, 256, 10);
	makejson("chest_movement", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.manual_count, buffer, 256, 10);
	makejson("manual_count", buffer);
	htmlReply += "\n}\n";

}

extern char WVSversion[];

void
sendStatus(void)
{
	char buffer[256];
	int i;

	htmlReply += " \"scenario\" : {\n";
	makejson("active", simmgr_shm->status.scenario.active);
	htmlReply += ",\n";
	makejson("start", simmgr_shm->status.scenario.start);
	htmlReply += ",\n";
	makejson("runtime", simmgr_shm->status.scenario.runtimeAbsolute);
	htmlReply += ",\n";
	makejson("runtimeScenario", simmgr_shm->status.scenario.runtimeScenario);
	htmlReply += ",\n";
	makejson("runtimeScene", simmgr_shm->status.scenario.runtimeScene);
	htmlReply += ",\n";
	makejson("clockDisplay", simmgr_shm->status.scenario.clockDisplay);
	htmlReply += ",\n";
	makejson("scene_name", simmgr_shm->status.scenario.scene_name);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.scenario.scene_id, buffer, 256, 10);
	makejson("scene_id", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.scenario.record, buffer, 256, 10);
	makejson("record", buffer);
	htmlReply += ",\n"; 
	if (strlen(simmgr_shm->status.scenario.error_message) > 0)
	{
		makejson("error_message", simmgr_shm->status.scenario.error_message);
		htmlReply += ",\n";
		simmgr_shm->status.scenario.error_message[0] = 0;
	}
	makejson("state", simmgr_shm->status.scenario.state);
	htmlReply += "\n},\n";
	htmlReply += " \"logfile\" : {\n";
	_itoa_s(simmgr_shm->logfile.active, buffer, 256, 10);
	makejson("active", buffer);
	htmlReply += ",\n";

	makejson("filename", simmgr_shm->logfile.filename);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->logfile.lines_written, buffer, 256, 10);
	makejson("lines_written", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"cardiac\" : {\n";
	makejson("rhythm", simmgr_shm->status.cardiac.rhythm);
	htmlReply += ",\n";
	makejson("vpc", simmgr_shm->status.cardiac.vpc);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pea, buffer, 256, 10);
	makejson("pea", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.vpc_freq, buffer, 256, 10);
	makejson("vpc_freq", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.vpc_delay, buffer, 256, 10);
	makejson("vpc_delay", buffer);
	htmlReply += ",\n";
	makejson("vfib_amplitude", simmgr_shm->status.cardiac.vfib_amplitude);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_ltoa_s(simmgr_shm->status.cardiac.avg_rate, buffer, 256, 10);
	makejson("avg_rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_rate, buffer, 256, 10);
	makejson("nibp_rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_read, buffer, 256, 10);
	makejson("nibp_read", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_linked_hr, buffer, 256, 10);
	makejson("nibp_linked_hr", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.nibp_freq, buffer, 256, 10);
	makejson("nibp_freq", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCount, buffer, 256, 10);
	makejson("pulseCount", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCountVpc, buffer, 256, 10);
	makejson("pulseCountVpc", buffer);
	htmlReply += ",\n";
	makejson("pwave", simmgr_shm->status.cardiac.pwave);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pr_interval, buffer, 256, 10);
	makejson("pr_interval", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.qrs_interval, buffer, 256, 10);
	makejson("qrs_interval", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.bps_sys, buffer, 256, 10);
	makejson("bps_sys", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.bps_dia, buffer, 256, 10);
	makejson("bps_dia", buffer);
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.right_dorsal_pulse_strength)
	{
	case 0:
		makejson("right_dorsal_pulse_strength", "none");
		break;
	case 1:
		makejson("right_dorsal_pulse_strength", "weak");
		break;
	case 2:
		makejson("right_dorsal_pulse_strength", "medium");
		break;
	case 3:
		makejson("right_dorsal_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.right_dorsal_pulse_strength, buffer, 256, 10);
		makejson("right_dorsal_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.left_dorsal_pulse_strength)
	{
	case 0:
		makejson("left_dorsal_pulse_strength", "none");
		break;
	case 1:
		makejson("left_dorsal_pulse_strength", "weak");
		break;
	case 2:
		makejson("left_dorsal_pulse_strength", "medium");
		break;
	case 3:
		makejson("left_dorsal_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.left_dorsal_pulse_strength, buffer, 256, 10);
		makejson("left_dorsal_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.right_femoral_pulse_strength)
	{
	case 0:
		makejson("right_femoral_pulse_strength", "none");
		break;
	case 1:
		makejson("right_femoral_pulse_strength", "weak");
		break;
	case 2:
		makejson("right_femoral_pulse_strength", "medium");
		break;
	case 3:
		makejson("right_femoral_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.right_femoral_pulse_strength, buffer, 256, 10);
		makejson("right_femoral_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	switch (simmgr_shm->status.cardiac.left_femoral_pulse_strength)
	{
	case 0:
		makejson("left_femoral_pulse_strength", "none");
		break;
	case 1:
		makejson("left_femoral_pulse_strength", "weak");
		break;
	case 2:
		makejson("left_femoral_pulse_strength", "medium");
		break;
	case 3:
		makejson("left_femoral_pulse_strength", "strong");
		break;
	default:	// Should never happen
		_itoa_s(simmgr_shm->status.cardiac.left_femoral_pulse_strength, buffer, 256, 10);
		makejson("left_femoral_pulse_strength", buffer);
		break;
	}
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.heart_sound_volume, buffer, 256, 10);
	makejson("heart_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.heart_sound_mute, buffer, 256, 10);
	makejson("heart_sound_mute", buffer);
	htmlReply += ",\n";
	makejson("heart_sound", simmgr_shm->status.cardiac.heart_sound);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.ecg_indicator, buffer, 256, 10);
	makejson("ecg_indicator", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.bp_cuff, buffer, 256, 10);
	makejson("bp_cuff", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.arrest, buffer, 256, 10);
	makejson("arrest", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"respiration\" : {\n";
	makejson("left_lung_sound", simmgr_shm->status.respiration.left_lung_sound);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.left_lung_sound_volume, buffer, 256, 10);
	makejson("left_lung_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.left_lung_sound_mute, buffer, 256, 10);
	makejson("left_lung_sound_mute", buffer);
	htmlReply += ",\n";
	makejson("right_lung_sound", simmgr_shm->status.respiration.right_lung_sound);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.right_lung_sound_volume, buffer, 256, 10);
	makejson("right_lung_sound_volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.right_lung_sound_mute, buffer, 256, 10);
	makejson("right_lung_sound_mute", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.inhalation_duration, buffer, 256, 10);
	makejson("inhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.exhalation_duration, buffer, 256, 10);
	makejson("exhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.breathCount, buffer, 256, 10);
	makejson("breathCount", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.spo2, buffer, 256, 10);
	makejson("spo2", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.etco2, buffer, 256, 10);
	makejson("etco2", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.awRR, buffer, 256, 10);
	makejson("awRR", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.etco2_indicator, buffer, 256, 10);
	makejson("etco2_indicator", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.spo2_indicator, buffer, 256, 10);
	makejson("spo2_indicator", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.chest_movement, buffer, 256, 10);
	makejson("chest_movement", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.manual_count, buffer, 256, 10);
	makejson("manual_count", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"auscultation\" : {\n";
	_itoa_s(simmgr_shm->status.auscultation.side, buffer, 256, 10);
	makejson("side", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.auscultation.row, buffer, 256, 10);
	makejson("row", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.auscultation.col, buffer, 256, 10);
	makejson("col", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"general\" : {\n";
	makejson("wvs_version", WVSversion);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.general.temperature, buffer, 256, 10);
	makejson("temperature", buffer);
	htmlReply += ",\n";
	makejson("temperature_units", simmgr_shm->status.general.temperature_units);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.general.temperature_enable, buffer, 256, 10);
	makejson("temperature_enable", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"vocals\" : {\n";
	makejson("filename", simmgr_shm->status.vocals.filename);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.vocals.repeat, buffer, 256, 10);
	makejson("repeat", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.vocals.volume, buffer, 256, 10);
	makejson("volume", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.vocals.play, buffer, 256, 10);
	makejson("play", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.vocals.mute, buffer, 256, 10);
	makejson("mute", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"pulse\" : {\n";
	_itoa_s(simmgr_shm->status.pulse.right_dorsal, buffer, 256, 10);
	makejson("right_dorsal", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.pulse.left_dorsal, buffer, 256, 10);
	makejson("left_dorsal", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.pulse.right_femoral, buffer, 256, 10);
	makejson("right_femoral", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.pulse.left_femoral, buffer, 256, 10);
	makejson("left_femoral", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.pulse.duration, buffer, 256, 10);
	makejson("duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.pulse.active, buffer, 256, 10);
	makejson("active", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"media\" : {\n";
	makejson("filename", simmgr_shm->status.media.filename);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.media.play, buffer, 256, 10);
	makejson("play", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"telesim\" : {\n";
	_itoa_s(simmgr_shm->status.telesim.enable, buffer, 256, 10);
	makejson("enable", buffer);
	htmlReply += ",\n";
	int vidCount = 0;
	for (i = 0; i < TSIM_WINDOWS; i++)
	{
		if (vidCount > 0)
		{
			htmlReply += ",\n";
		}
		//printf("TSIM_WINDOW %d %s %d\n", i, simmgr_shm->status.telesim.vid[i].name, simmgr_shm->status.telesim.vid[i].next);
		sprintf_s(buffer, 256, "\"%d\" : {\n", vidCount);
		htmlReply += buffer;
		vidCount++;

		//makejson("name", "");
		
		
		if ( strlen(simmgr_shm->status.telesim.vid[i].name) > 0 )
		{
			makejson("name", simmgr_shm->status.telesim.vid[i].name);
		}
		else
		{
			makejson("name", "");
		}
		htmlReply += ",\n";
		
		_itoa_s(simmgr_shm->status.telesim.vid[i].command, buffer, 256, 10);
		makejson("command", buffer);
		htmlReply += ",\n";
		_gcvt_s(buffer, sizeof(buffer), simmgr_shm->status.telesim.vid[i].param, 8);
		makejson("param", buffer);
		htmlReply += ",\n";
		_itoa_s(simmgr_shm->status.telesim.vid[i].next, buffer, 256, 10);
		makejson("next", buffer);
		
		htmlReply += "  }";
	}
	if (vidCount > 0)
	{
		htmlReply += "\n";
	}
	htmlReply += "},\n";

	htmlReply += " \"cpr\" : {\n";

	_i64toa_s(simmgr_shm->status.cpr.last, buffer, 256, 10);
	makejson("last", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cpr.running, buffer, 256, 10);
	makejson("running", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cpr.compression, buffer, 256, 10);
	makejson("compression", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cpr.release, buffer, 256, 10);
	makejson("release", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"defibrillation\" : {\n";
	_i64toa_s(simmgr_shm->status.defibrillation.last, buffer, 256, 10);
	makejson("last", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.defibrillation.shock, buffer, 256, 10);
	makejson("shock", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.defibrillation.energy, buffer, 256, 10);
	makejson("energy", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"debug\" : {\n";
	_i64toa_s(simmgr_shm->server.msec_time, buffer, 256, 10);
	makejson("msec", buffer);
	htmlReply += ",\n";
	_ltoa_s(simmgr_shm->status.cardiac.avg_rate, buffer, 256, 10);
	makejson("avg_rate", buffer);
	htmlReply += ",\n";
	extern ULONGLONG breathInterval;
	_i64toa_s(breathInterval, buffer, 256, 10);
	makejson("breathInterval", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->server.dbg2, buffer, 256, 10);
	makejson("debug2", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->server.dbg3, buffer, 256, 10);
	makejson("debug3", buffer);
	htmlReply += "\n},\n";

	htmlReply += "\"controllers\" : {\n";

	int ctrlCount = 0;
	for (i = 0; i < MAX_CONTROLLERS; i++)
	{
		if (simmgr_shm->simControllers[i].allocated)
		{
			if (ctrlCount > 0)
			{
				htmlReply += ",\n";
			}
			ctrlCount++;
			_itoa_s(i + 1, buffer, 256, 10);
			makejson(buffer, simmgr_shm->simControllers[i].ipAddr);
		}
	}
	if (ctrlCount > 0)
	{
		htmlReply += "\n";
	}
	htmlReply += "},\n";

	htmlReply += "\"controllerVersions\" : {\n";
	ctrlCount = 0;

	for (i = 0; i < MAX_CONTROLLERS; i++)
	{
		if (simmgr_shm->simControllers[i].allocated)
		{
			if (ctrlCount > 0)
			{
				htmlReply += ",\n";
			}
			ctrlCount++;
			_itoa_s(i + 1, buffer, 256, 10);
			makejson(buffer, simmgr_shm->simControllers[i].version);
		}
	}
	if (ctrlCount > 0)
	{
		htmlReply += "\n";
	}
	htmlReply += "}\n";
}

void
sendQuickStatus(void)
{
	char buffer[256];


	htmlReply += " \"cardiac\" : {\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCount, buffer, 256, 10);
	makejson("pulseCount", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.pulseCountVpc, buffer, 256, 10);
	makejson("pulseCountVpc", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.cardiac.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_ltoa_s(simmgr_shm->status.cardiac.avg_rate, buffer, 256, 10);
	makejson("avg_rate", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"respiration\" : {\n";
	_itoa_s(simmgr_shm->status.respiration.breathCount, buffer, 256, 10);
	makejson("breathCount", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.inhalation_duration, buffer, 256, 10);
	makejson("inhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.exhalation_duration, buffer, 256, 10);
	makejson("exhalation_duration", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.rate, buffer, 256, 10);
	makejson("rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.awRR, buffer, 256, 10);
	makejson("awRR", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->status.respiration.manual_count, buffer, 256, 10);
	makejson("manual_count", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"defibrillation\" : {\n";
	_itoa_s(simmgr_shm->status.defibrillation.shock, buffer, 256, 10);
	makejson("shock",  buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"cpr\" : {\n";
	_itoa_s(simmgr_shm->status.cpr.running, buffer, 256, 10);
	makejson("running", buffer);
	htmlReply += "\n},\n";

	htmlReply += " \"debug\" : {\n";
	_i64toa_s(simmgr_shm->server.msec_time, buffer, 256, 10);
	makejson("msec", buffer);
	htmlReply += ",\n";
	_ltoa_s(simmgr_shm->status.cardiac.avg_rate, buffer, 256, 10);
	makejson("avg_rate", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->server.dbg1, buffer, 256, 10);
	makejson("debug1", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->server.dbg2, buffer, 256, 10);
	makejson("debug2", buffer);
	htmlReply += ",\n";
	_itoa_s(simmgr_shm->server.dbg3, buffer, 256, 10);
	makejson("debug3", buffer);
	htmlReply += "\n}\n";
}
void replaceAll(char* args, size_t len, const char* needle, const char replace)
{
	char* src;
	char* next;
	string lbuf;
	int sts;
	int count = 0;

	if (strlen(needle) == 1)
	{
		size_t i;
		for (i = 0; i < len; i++)
		{
			if ( args[i] == needle[0] )
			{
				args[i] = replace;
			}
		}
	}
	else
	{
		src = args;
		while (src)
		{
			next = strstr(src, needle);
			if (next)
			{
				next[0] = replace;
				next[1] = 0;
				lbuf += src;
				src = next + strlen(needle);
				count++;
			}
			else
			{
				lbuf += src;
				src = NULL;
			}
		}
		if (count > 0)
		{
			if (args &&
				strlen(args) > 0 &&
				len > 0 &&
				lbuf.length() > 0)
			{
				sts = sprintf_s(args,
					len,
					"%s",
					lbuf.c_str());
			}
		}
	}
}
