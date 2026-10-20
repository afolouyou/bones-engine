// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "date_time.h"

#include <ctime>

DateTime DateTime::GetNow()
{
    DateTime dateTime;
    std::time_t t = std::time(0);   // get time now
    std::tm* now = localtime(&t);

	if (now)
	{
		dateTime.second = now->tm_sec;
		dateTime.minute = now->tm_min;
		dateTime.hour = now->tm_hour;
		dateTime.day = now->tm_mday;
		dateTime.month = now->tm_mon + 1;
		dateTime.year = now->tm_year + 1900;
	}
    return dateTime;
}

std::string DateTime::ToString() const
{
	return std::to_string(hour) + ":" + std::to_string(minute) + ":" + std::to_string(second) + " " + std::to_string(day) + "/" + std::to_string(month) + "/" + std::to_string(year);
}
