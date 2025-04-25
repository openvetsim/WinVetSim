#pragma once

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
#define DEFAULT_HTML_PATH			"WinVetSim\\html"

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
