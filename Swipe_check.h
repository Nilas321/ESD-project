#ifndef SWIPE_CHECK_H
#define SWIPE_CHECK_H

typedef enum {
    SWIPE_NONE = 0,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT
} Swipe_Dir_t;

// Function Prototype
void StartSwipeCheck(void);

#endif