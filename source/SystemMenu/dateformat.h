#ifndef WSM_DATE_FORMAT_H
#define WSM_DATE_FORMAT_H

#include <cstdio>
#include <cstddef>

namespace DateFormat
{
enum Order { Automatic, DayMonthYear, MonthDayYear, YearMonthDay, OrderCount };
inline int Normalize(int order)
{
    return order >= Automatic && order < OrderCount ? order : Automatic;
}
inline int Step(int order, int direction)
{
    return (Normalize(order) + (direction < 0 ? OrderCount - 1 : 1)) % OrderCount;
}
inline const char *Label(int order)
{
    static const char *labels[] = {"Automatic", "DD/MM/YYYY", "MM/DD/YYYY", "YYYY/MM/DD"};
    return labels[Normalize(order)];
}
// Automatic preserves the caller's native compact date. Explicit choices
// include a four-digit year. This formats text only; it never changes time.
inline void Format(char *out, size_t capacity, int order, int year, int month,
    int day, bool nativeMonthFirst, bool nativeZeroPad)
{
    if(!out || !capacity) return;
    switch(Normalize(order))
    {
    case DayMonthYear: std::snprintf(out,capacity,"%02d/%02d/%04d",day,month,year); break;
    case MonthDayYear: std::snprintf(out,capacity,"%02d/%02d/%04d",month,day,year); break;
    case YearMonthDay: std::snprintf(out,capacity,"%04d/%02d/%02d",year,month,day); break;
    default:
        std::snprintf(out,capacity,nativeZeroPad ? "%02d/%02d" : "%d/%d",
            nativeMonthFirst ? month : day, nativeMonthFirst ? day : month);
        break;
    }
}
}
#endif
