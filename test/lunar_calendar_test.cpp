#include <lunar_calendar.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
    for (auto date : {lunar::vietnamese(10,2,2024), lunar::vietnamese(29,1,2025), lunar::vietnamese(17,2,2026)})
        assert(date.day==1 && date.month==1 && !date.leap);
    const auto leap=lunar::vietnamese(21,3,2004);
    assert(leap.day==1 && leap.month==2 && leap.leap);
    for (int year=2020;year<=2030;++year) for(int month=1;month<=12;++month) for(int day=1;day<=28;++day) {
        const auto value=lunar::vietnamese(day,month,year);
        assert(value.day>=1 && value.day<=30 && value.month>=1 && value.month<=12);
    }
    assert(lunar::vietnamese(1,1,2100).day==0);
    puts("Vietnamese lunar dates: Tet, leap-month boundary and date bounds passed");
}
