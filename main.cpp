#define DEBUG

#include <iostream>
#include <fstream>
#include <vector>

enum ExitStatus
{
	OK = 0,
	OPEN_MACADDRESSES_FILE_OPEN_FAILED = 1,
	WRONG_MAC_ADDRESS = 2,
};

std::vector<std::string> macAddressesVector;

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

int main()
{
	printf("BlueDetector started...\n");

	std::fstream macAdressesFile;
	macAdressesFile.open("macAddresses.txt", std::fstream::in);
	if (macAdressesFile.fail())
	{
		printf("macAddresses.txt file open failed. Exiting...\n");
		exit(OPEN_MACADDRESSES_FILE_OPEN_FAILED);
	}

	std::string tempMac = "";
	std::getline(macAdressesFile, tempMac);
	while (!macAdressesFile.eof());
	{
		if (!IsValidMacAddress(tempMac.c_str()))
		{
			printf("Wrong MAC address: \"%s\n\". Exiting...", tempMac);
			exit(WRONG_MAC_ADDRESS);
		}

		macAddressesVector.push_back(tempMac);

		std::getline(macAdressesFile, tempMac);
	}



	getchar();
	return 0;
}