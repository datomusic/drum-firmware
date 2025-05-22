#include "../../musin/ui/keypad_hc138.h"
#include "../hal/mock_hardware.h"
#include "../test_support.h"
#include <array>
#include <vector>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

// Observer class to capture keypad events
class KeypadObserver : public etl::observer<musin::ui::KeypadEvent> {
public:
    std::vector<musin::ui::KeypadEvent> events;

    void notification(const musin::ui::KeypadEvent& event) override {
        events.push_back(event);
    }

    void clear() {
        events.clear();
    }
};

// Test fixture to reset mock hardware state before each test
struct KeypadFixture {
    KeypadFixture() {
        mock::reset_hardware_state();
    }
};

TEST_CASE_METHOD(KeypadFixture, "Keypad_HC138 initialization", "[keypad]") {
    SECTION("Basic initialization") {
        std::array<uint32_t, 3> decoder_pins = {1, 2, 3};
        std::array<uint32_t, 4> col_pins = {5, 6, 7, 8};
        
        musin::ui::Keypad_HC138<2, 4> keypad(decoder_pins, col_pins);
        keypad.init();
        
        // Verify decoder pins are initialized as outputs
        for (auto pin : decoder_pins) {
            REQUIRE(mock::gpio_states[pin].initialized);
            REQUIRE(mock::gpio_states[pin].direction == mock::GPIO_OUT);
        }
        
        // Verify column pins are initialized as inputs
        for (auto pin : col_pins) {
            REQUIRE(mock::gpio_states[pin].initialized);
            REQUIRE(mock::gpio_states[pin].direction == mock::GPIO_IN);
        }
    }
}

TEST_CASE_METHOD(KeypadFixture, "Keypad_HC138 key press detection", "[keypad]") {
    std::array<uint32_t, 3> decoder_pins = {1, 2, 3};
    std::array<uint32_t, 4> col_pins = {5, 6, 7, 8};
    
    musin::ui::Keypad_HC138<2, 4> keypad(decoder_pins, col_pins);
    keypad.init();
    
    KeypadObserver observer;
    keypad.attach_observer(observer);
    
    SECTION("Single key press and release") {
        // Simulate key press at row 0, col 1
        // When row 0 is selected (decoder pins = 000), column 1 reads LOW
        
        // First scan - no keys pressed
        keypad.scan();
        
        // Simulate key press
        // When row 0 is selected (decoder pins = 000), column 1 reads HIGH (pressed)
        mock::gpio_states[6].value = true; // Column 1 (pin 6) reads HIGH when not selected
        
        // Advance time for debounce
        mock::current_time_us += 10000; // 10ms
        
        // Scan to detect press
        keypad.scan();
        
        // Verify key state
        REQUIRE_FALSE(keypad.is_pressed(0, 0));
        REQUIRE_FALSE(keypad.is_pressed(0, 1)); // Still debouncing
        
        // Advance time past debounce period
        mock::current_time_us += 10000; // 10ms
        
        // Scan again to confirm press
        keypad.scan();
        
        // Verify key is now pressed
        REQUIRE(keypad.is_pressed(0, 1));
        REQUIRE(keypad.was_pressed(0, 1));
        REQUIRE_FALSE(keypad.is_held(0, 1)); // Not held yet
        
        // Verify press event was sent
        REQUIRE(observer.events.size() == 1);
        REQUIRE(observer.events[0].row == 0);
        REQUIRE(observer.events[0].col == 1);
        REQUIRE(observer.events[0].type == musin::ui::KeypadEvent::Type::Press);
        
        // Clear events
        observer.clear();
        
        // Advance time past hold threshold
        mock::current_time_us += 600000; // 600ms (past the 500ms hold time)
        
        // Scan to detect hold
        keypad.scan();
        
        // Verify key is now held
        REQUIRE(keypad.is_pressed(0, 1));
        REQUIRE(keypad.is_held(0, 1));
        
        // Verify hold event was sent
        REQUIRE(observer.events.size() == 1);
        REQUIRE(observer.events[0].row == 0);
        REQUIRE(observer.events[0].col == 1);
        REQUIRE(observer.events[0].type == musin::ui::KeypadEvent::Type::Hold);
        
        // Clear events
        observer.clear();
        
        // Simulate key release
        mock::gpio_states[6].value = false;
        
        // Advance time
        mock::current_time_us += 10000; // 10ms
        
        // Scan to detect release (debouncing)
        keypad.scan();
        
        // Verify key is still considered pressed during debounce
        REQUIRE(keypad.is_pressed(0, 1));
        
        // Advance time past debounce period
        mock::current_time_us += 10000; // 10ms
        
        // Scan again to confirm release
        keypad.scan();
        
        // Verify key is now released
        REQUIRE_FALSE(keypad.is_pressed(0, 1));
        REQUIRE(keypad.was_released(0, 1));
        
        // Verify release event was sent
        REQUIRE(observer.events.size() == 1);
        REQUIRE(observer.events[0].row == 0);
        REQUIRE(observer.events[0].col == 1);
        REQUIRE(observer.events[0].type == musin::ui::KeypadEvent::Type::Release);
    }
    
    SECTION("Tap detection") {
        // Simulate a quick tap (press and release within tap time)
        
        // Simulate key press at row 1, col 2
        mock::gpio_states[7].value = true; // Column 2 (pin 7) reads HIGH when not selected
        
        // Advance time for debounce
        mock::current_time_us += 10000; // 10ms
        
        // Scan to detect press
        keypad.scan();
        
        // Advance time past debounce period
        mock::current_time_us += 10000; // 10ms
        
        // Scan again to confirm press
        keypad.scan();
        
        // Clear events
        observer.clear();
        
        // Simulate key release quickly (within tap time)
        mock::current_time_us += 30000; // 30ms (less than tap time)
        mock::gpio_states[7].value = false;
        
        // Scan to detect release (debouncing)
        keypad.scan();
        
        // Advance time past debounce period
        mock::current_time_us += 10000; // 10ms
        
        // Scan again to confirm release
        keypad.scan();
        
        // Verify tap event was sent (in addition to release)
        REQUIRE(observer.events.size() == 2);
        
        // Find the tap event
        bool found_tap = false;
        for (const auto& event : observer.events) {
            if (event.type == musin::ui::KeypadEvent::Type::Tap) {
                found_tap = true;
                REQUIRE(event.row == 1);
                REQUIRE(event.col == 2);
            }
        }
        
        REQUIRE(found_tap);
    }
    
    SECTION("Multiple keys pressed simultaneously") {
        // Simulate two keys pressed at the same time
        
        // Simulate key press at row 0, col 0 and row 1, col 3
        mock::gpio_states[5].value = true; // Column 0 (pin 5) reads HIGH when not selected
        mock::gpio_states[8].value = true; // Column 3 (pin 8) reads HIGH when not selected
        
        // Advance time for debounce
        mock::current_time_us += 10000; // 10ms
        
        // Scan to detect press
        keypad.scan();
        
        // Advance time past debounce period
        mock::current_time_us += 10000; // 10ms
        
        // Scan again to confirm press
        keypad.scan();
        
        // Verify both keys are pressed
        REQUIRE(keypad.is_pressed(0, 0));
        REQUIRE(keypad.is_pressed(1, 3));
        
        // Verify press events were sent for both keys
        REQUIRE(observer.events.size() == 2);
        
        // Check that we have events for both keys
        bool found_key1 = false;
        bool found_key2 = false;
        
        for (const auto& event : observer.events) {
            if (event.row == 0 && event.col == 0) {
                found_key1 = true;
            }
            if (event.row == 1 && event.col == 3) {
                found_key2 = true;
            }
        }
        
        REQUIRE(found_key1);
        REQUIRE(found_key2);
    }
}