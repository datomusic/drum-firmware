#ifndef HARDWARE_REDIRECT_H
#define HARDWARE_REDIRECT_H

// This header is used to redirect hardware includes to our mock implementation
// It should be included before any other hardware includes

#include "../hal/mock_hardware.h"

// Define hardware constants to match the mock implementation
#define GPIO_IN mock::GPIO_IN
#define GPIO_OUT mock::GPIO_OUT

#endif // HARDWARE_REDIRECT_H