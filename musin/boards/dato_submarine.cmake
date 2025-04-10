# Add these definitions to the board configuration:
set(PICO_DEFAULT_TIMER_INSTANCE 0)  # Explicitly select Timer 0 as default
set(PICO_DEFAULT_TIMER 0)           # Match timer instance used in Pico SDK

# Also ensure these RP2350-specific flags are set:
set(PICO_RP2350 1)
set(PICO_PLATFORM rp2350)
set(PICO_ARCH arm)
