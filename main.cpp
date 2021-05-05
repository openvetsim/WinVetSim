/*
 * VetSim.cpp
 *
 * SimMgr applicatiopn
 *
 * This file is part of the sim-mgr distribution.
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

//using namespace System;

//int main(array<System::String ^> ^args)
//{
//    return 0;
//}

//#include "Form1.h"
//#include "MyForm.h"

//using namespace System::Windows::Forms;

//[STAThread]
int main(int argc, char* argv[], char* envp[])
{
	int i;
	char* ptr;

	if (argc > 1)
	{
		for (i = 1; i < argc; i++)
		{
			if (strncmp(argv[i], "-v", 2) == 0 || strncmp(argv[i], "-V", 2) == 0 ||
				strncmp(argv[i], "\\v", 2) == 0 || strncmp(argv[i], "\\V", 2) == 0 ||
				strncmp(argv[i], "/v", 2) == 0 || strncmp(argv[i], "/V", 2) == 0 ||
				strncmp(argv[i], "--version", 9) == 0 || strncmp(argv[i], "--Version", 9) == 0)
			{
				ptr = argv[0];
				size_t c = 0;
				while (c < strlen(argv[0]))
				{
					if (argv[0][c] == '\\')
					{
						ptr = &argv[0][c + 1];
					}
					c++;
				}
				printf("%s: Version %d.%d\n", ptr, SIMMGR_VERSION_MAJ, SIMMGR_VERSION_MIN);
				exit(0);
			}
			else
			{
				printf("Unrecognized argumment: \"%s\"\n", argv[i]);
				exit(-1);
			}
		}
	}
	// Start the application
	vetsim();

	//Application::EnableVisualStyles();
	//Application::SetCompatibleTextRenderingDefault(false);
	//Application::Run(gcnew Winform1::MyForm());

	return 0;
}