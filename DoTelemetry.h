// DoTelemetry.h : Functions for processing the telemetry data
//
#pragma once

#include "maniaplanet_telemetry.h"
#include <vector>
using std::vector;
using namespace NManiaPlanet;

struct STelemetryData
{
	STelemetry Current;		// Contains the most recent telemetry data records from the shared memory
	STelemetry Previous;	// Contains saved values of the used telemetry data records (to test the live data for changes)
};

void DoTelemetry(STelemetryData*);
void InitTelemetryData(STelemetryData*);
