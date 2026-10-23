#ifndef WSM_DATE_FORMAT_H
#define WSM_DATE_FORMAT_H

#include <cstdio>
#include <cstddef>

namespace DateFormat
{
enum Order { Automatic, DayMonth, MonthDay, OrderCount };
inline int Normalize(int order)
{
    // Old value 3 was year/month/day. Preserve its month/day ordering when
    // migrating to compact dates, without adding a year to the footer.
    if(order == 3) return MonthDay;
    return order >= Automatic && order < OrderCount ? order : Automatic;
}
inline int Step(int order, int direction)
{
    return (Normalize(order) + (direction < 0 ? OrderCount - 1 : 1)) % OrderCount;
}
inline const char *Label(int order)
{
    static const char *labels[] = {"Automatic", "DD.MM", "MM.DD"};
    return labels[Normalize(order)];
}
// Wii language IDs: Japanese uses month/day; English does so in the USA.
// The European languages (including European English) use day/month.
inline bool AutomaticMonthFirst(int language, bool usRegion)
{
    return language == 0 || (language == 1 && usRegion);
}
// Date order only changes placement, not the fields or the console clock.
inline void Format(char *out, size_t capacity, int order, int month,
    int day, bool automaticMonthFirst)
{
    if(!out || !capacity) return;
    const int normalized = Normalize(order);
    const bool monthFirst = normalized == MonthDay
        || (normalized == Automatic && automaticMonthFirst);
    std::snprintf(out,capacity,"%02d.%02d",monthFirst ? month : day,
        monthFirst ? day : month);
}
}
#endif
