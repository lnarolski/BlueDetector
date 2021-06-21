//#define DEBUG
#define DOMOTICZ // Send detection results to Domoticz home automation system
#define V4l2RTSPSERVER // Turn on/off V4l2RTSPSERVER based on detection results

#define TIME_BETWEEN_PINGS_SEC 30
#define MAX_PING_ATTEMPTS 7

#include <iostream>
#include <fstream>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <future>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "Domoticz.h"

using namespace boost::interprocess;

enum ExitStatus
{
	OK = 0,
	MACADDRESSES_FILE_OPEN_FAILED = 1,
	WRONG_MAC_ADDRESS = 2,
};

std::vector<std::string> macAddressesVector;

bool stopApplication = false;

bool IsValidMacAddress(const char* mac) { //Oded
	int i = 0;
	int s = 0;

	while (*mac) {
		if (isxdigit(*mac)) {
			i++;
		}
		else if (*mac == ':' || *mac == '-') {

			if (i == 0 || i / 2 - 1 != s)
				break;

			++s;
		}
		else {
			s = -1;
		}

		++mac;
	}

	return (i == 12 && (s == 5 || s == 0));
}

void SignalHandler(int signal)
{
	stopApplication = true;
}

//Map the whole shared memory in this process
mapped_region detectionResultRegion;

void BluetoothDevicesPing(bool pingSuccess) // Action when Bluetooth device(s) is found
{
	memset(detectionResultRegion.get_address(), pingSuccess, detectionResultRegion.get_size());

#ifdef DEBUG
	if (pingSuccess)
		printf("Bluetooth device(s) found\n");
	else
		printf("Bluetooth device(s) NOT found\n");
#endif // DEBUG

#ifdef DOMOTICZ
	std::async(SendDetectionResultToDomoticz, pingSuccess);
#endif // DOMOTICZ

	static bool previousPingSuccessValue = false;
	if (pingSuccess != previousPingSuccessValue)
	{
#ifdef V4l2RTSPSERVER
		if (pingSuccess)
		{
			system("systemctl stop v4l2rtspserver.service");
#ifdef DEBUG
			printf("v4l2rtspserver stopped.\n");
#endif // DEBUG
		}
		else
		{
			system("systemctl start v4l2rtspserver.service");
#ifdef DEBUG
			printf("v4l2rtspserver started.\n");
#endif // DEBUG
		}
#endif // V4l2RTSPSERVER

		previousPingSuccessValue = pingSuccess;
	}
}

int main()
{
	printf("BlueDetector started...\n");

	std::signal(SIGABRT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);
	std::signal(SIGKILL, SignalHandler);
	std::signal(SIGSEGV, SignalHandler);

	std::fstream macAdressesFile;
	macAdressesFile.open("macAddresses.txt", std::fstream::in);
	if (macAdressesFile.fail())
	{
		printf("macAddresses.txt file open failed. Exiting...\n");
		exit(MACADDRESSES_FILE_OPEN_FAILED);
	}

	std::string tempMac = "";

	while (std::getline(macAdressesFile, tempMac))
	{
		if (!IsValidMacAddress(tempMac.c_str()))
		{
			printf("Wrong MAC address: \"%s\n\". Exiting...", tempMac);
			exit(WRONG_MAC_ADDRESS);
		}

		macAddressesVector.push_back(tempMac);
	}
	macAdressesFile.close();

	// Create shared memory region
	shared_memory_object shm;
	shm.remove("detectionResult");
	shm = shared_memory_object(create_only, "detectionResult", read_write);
	shm.truncate(sizeof(bool));
	detectionResultRegion = mapped_region(shm, read_write);
	//

	system("systemctl start v4l2rtspserver.service");

	uint32_t attempts = 0;
	uint32_t waitTime = TIME_BETWEEN_PINGS_SEC;
	while (!stopApplication)
	{
		if (waitTime == TIME_BETWEEN_PINGS_SEC)
		{
			bool pingSuccess = false;
			for (size_t i = 0; i < macAddressesVector.size(); i++)
			{
				std::string command = "l2ping " + macAddressesVector[i] + " -c 1";
				int pingReturnCode = system(command.c_str());  // Need l2ping from BlueZ package

#ifdef DEBUG
				printf("Called command \"%s\" and got return code: %d\n", command.c_str(), pingReturnCode);
#endif // DEBUG

				if (pingReturnCode == 0)
				{
					pingSuccess = true;
					break;
				}
			}

			if (pingSuccess)
			{
#ifdef DEBUG
				printf("Ping success.\n");
#endif // DEBUG
				BluetoothDevicesPing(true);
			}
			else if (!pingSuccess && attempts == MAX_PING_ATTEMPTS)
			{
#ifdef DEBUG
				printf("Maximum number of attempts reached.\n");
#endif // DEBUG
				BluetoothDevicesPing(false);
			}
			else
			{
#ifdef DEBUG
				printf("Ping failed, another attempt...\n");
#endif // DEBUG
				++attempts;
			}

			waitTime = 0;
		}
		else
		{
			++waitTime;
		}

		sleep(1);
	}

	return EXIT_SUCCESS;
}