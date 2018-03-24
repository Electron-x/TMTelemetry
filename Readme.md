# TrackMania Telemetry Monitor #

**TrackMania Telemetry Monitor (TM²)** is a tool for Microsoft Windows that displays real-time telemetry data from the [TrackMania](https://www.trackmania.com/) racing game.

The application lists the sector and/or checkpoint times of a race in a table, supplemented by a few statistics. In addition, a selection of live data is displayed in the status bar:

![Screenshot of TM²](http://www.wolfgang-rolke.de/gbxdump/telemetry.png)

The application does not log any data or perform any analyses. Also no gauges or line graphs are displayed.

The source code is provided as an example and should show how to access the game data via the [telemetry interface](https://wiki.xaseco.org/wiki/Telemetry_interface) of [Maniaplanet 4](https://www.maniaplanet.com/) or [Trackmania Turbo](https://www.ubisoft.com/en-gb/game/trackmania-turbo/).

It's a generic C/C++ Win32 desktop project created with Visual Studio 2013. Since no external libraries are used, the project should be built without problems. An older VS2005 project file (.vcproj) and two Visual Studio Installer projects for x86 and x64 are also included.
