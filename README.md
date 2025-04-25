# WinVetSim
## Windows based version of the Open VetSim project
All projects within the archive are licensed under GLP v3.0. See LICENSE.

WinVetSim was developed using Microsoft Visual Studio Community 2022


## Release Notes
### WinVetSim SIMMGR Version 2.2 4/25/2025
* Add improved Group Trigger support.<br>
	&nbsp;&nbsp;&nbsp;&nbsp;Move xml parse functions from scenario.cpp to new file, scenario_xml.cpp
	
* Add new trigger targets to allow trigger on calculated rates rather than programmed rates:<br>
	&nbsp;&nbsp;&nbsp;&nbsp;cardiac:avg_rate<br>
	&nbsp;&nbsp;&nbsp;&nbsp;respiration:awRR
	
* Change SimManager version from Major.Minor.BuildNumber to Major.Minor.DateCode<br>
	&nbsp;&nbsp;&nbsp;&nbsp;Date code is YYYYMMDDHH

* When registry key WinVetSim is not found, locate the html directories by looking in /users/Public/WinVetSim.

* Update PHP executable search path 