#pragma once
namespace lunar {
struct Date { int day; int month; int year; bool leap; };
// Modern Vietnamese calendar (UTC+7), Gregorian input, 2000..2099.
Date vietnamese(int day, int month, int year);
}
