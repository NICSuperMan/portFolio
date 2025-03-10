#pragma once
#include <windows.h>
struct MonitorData
{
	LONG total_;
	LONG cnt_;
	LONG min_;
	LONG max_;
	MonitorData()
		:total_{0},cnt_{0},min_{LONG_MAX},max_{LONG_MIN}
	{}
};
