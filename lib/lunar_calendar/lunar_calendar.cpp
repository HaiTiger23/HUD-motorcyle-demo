#include "lunar_calendar.h"
#include <cmath>

// Astronomical rules described by Ho Ngoc Duc (Meeus approximation):
// https://honguyenviet.com/amlich/calrules.html . Computation is local, UTC+7.
namespace lunar {
namespace {
constexpr double PI = 3.14159265358979323846;
int integer(double value) { return int(std::floor(value)); }
int julian(int d, int m, int y) {
    const int a = (14-m)/12, year = y+4800-a, month = m+12*a-3;
    return d+(153*month+2)/5+365*year+year/4-year/100+year/400-32045;
}
int moon(int k) {
    const double t=k/1236.85, t2=t*t, t3=t2*t, r=PI/180;
    const double sun=(359.2242+29.10535608*k-.0000333*t2-.00000347*t3)*r;
    const double anomaly=(306.0253+385.81691806*k+.0107306*t2+.00001236*t3)*r;
    const double latitude=(21.2964+390.67050646*k-.0016528*t2-.00000239*t3)*r;
    const double mean=2415020.75933+29.53058868*k+.0001178*t2-.000000155*t3+
        .00033*std::sin((166.56+132.87*t-.009173*t2)*r);
    const double correction=(.1734-.000393*t)*std::sin(sun)+.0021*std::sin(2*sun)-
        .4068*std::sin(anomaly)+.0161*std::sin(2*anomaly)-.0004*std::sin(3*anomaly)+
        .0104*std::sin(2*latitude)-.0051*std::sin(sun+anomaly)-.0074*std::sin(sun-anomaly)+
        .0004*std::sin(2*latitude+sun)-.0004*std::sin(2*latitude-sun)-
        .0006*std::sin(2*latitude+anomaly)+.0010*std::sin(2*latitude-anomaly)+.0005*std::sin(2*anomaly+sun);
    const double delta=-.000278+.000265*t+.000262*t2;
    return integer(mean+correction-delta+.5+7.0/24);
}
int sector(int day) {
    const double t=(day-2451545.5-7.0/24)/36525, t2=t*t, r=PI/180;
    const double anomaly=(357.52910+35999.05030*t-.0001559*t2-.00000048*t*t2)*r;
    double angle=(280.46645+36000.76983*t+.0003032*t2+
        (1.914600-.004817*t-.000014*t2)*std::sin(anomaly)+
        (.019993-.000101*t)*std::sin(2*anomaly)+.000290*std::sin(3*anomaly))*r;
    angle-=2*PI*std::floor(angle/(2*PI));
    return integer(angle/PI*6);
}
int month11(int year) {
    const int k=integer((julian(31,12,year)-2415021)/29.530588853);
    const int start=moon(k);
    return sector(start)>=9 ? moon(k-1) : start;
}
}
Date vietnamese(int day, int month, int year) {
    if (year<2000 || year>2099 || month<1 || month>12 || day<1 || day>31) return {0,0,0,false};
    const int number=julian(day,month,year), k=integer((number-2415021.076998695)/29.530588853);
    int start=moon(k+1); if (start>number) start=moon(k);
    int a=month11(year), b=a, lunarYear=year;
    if (a>=start) a=month11(year-1); else { b=month11(year+1); ++lunarYear; }
    const int diff=integer((start-a)/29.0);
    int lunarMonth=diff+11; bool leap=false;
    if (b-a>365) {
        const int base=integer((a-2415021.076998695)/29.530588853+.5);
        int i=1, previous=sector(moon(base+1)), current;
        do { ++i; current=sector(moon(base+i)); if (current==previous) break; previous=current; } while (i<14);
        const int offset=i-1;
        if (diff>=offset) { lunarMonth=diff+10; leap=diff==offset; }
    }
    if (lunarMonth>12) lunarMonth-=12;
    if (lunarMonth>=11 && diff<4) --lunarYear;
    return {number-start+1,lunarMonth,lunarYear,leap};
}
}
