#ifndef WSM_PAGE_BUTTON_REPEAT_H
#define WSM_PAGE_BUTTON_REPEAT_H

#include <stdint.h>

// Navigation only: keep action shortcuts and camera zoom edge/held semantics.
class PageButtonRepeat
{
public:
    PageButtonRepeat() : direction(0), next(0) {}
    int Update(bool plus, bool minus, uint64_t now)
    {
        const int wanted = plus == minus ? 0 : plus ? 1 : -1;
        if(wanted != direction)
        {
            direction = wanted;
            next = now + 450;
            return 0; // The ordinary buttons-down event handles the first tap.
        }
        if(!direction || now < next) return 0;
        next = now + 200; // Never catch up with a burst after a stalled frame.
        return direction;
    }
private:
    int direction;
    uint64_t next;
};

#endif
