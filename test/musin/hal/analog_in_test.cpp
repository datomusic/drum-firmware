#include "../../musin/hal/analog_in.h"
#include "../test_support.h"
#include "mock_hardware.h"
#include <array>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

// Test fixture to reset mock hardware state before each test
struct AnalogInFixture {
    AnalogInFixture() {
        mock::reset_hardware_state();
    }
};

TEST_CASE_METHOD(AnalogInFixture, "AnalogIn initialization and reading", "[analog_in]") {
    SECTION("Basic initialization") {
        musin::hal::AnalogIn analog_in(26); // ADC0
        analog_in.init();
        
        // Verify ADC was initialized
        REQUIRE(mock::adc_state.initialized);
        
        // Verify GPIO was initialized
        REQUIRE(mock::gpio_states[26].initialized);
        REQUIRE(mock::gpio_states[26].direction == mock::GPIO_IN);
    }
    
    SECTION("Temperature sensor initialization") {
        musin::hal::AnalogIn analog_in(29, true); // ADC3 with temp sensor
        analog_in.init();
        
        // Verify temp sensor was enabled
        REQUIRE(mock::adc_state.temp_sensor_enabled);
    }
    
    SECTION("Reading raw values") {
        musin::hal::AnalogIn analog_in(27); // ADC1
        analog_in.init();
        
        // Set mock ADC value
        mock::adc_state.channel_values[1] = 2048; // Half of max value
        
        // Read and verify
        REQUIRE(analog_in.read_raw() == 2048);
        REQUIRE(mock::adc_state.selected_input == 1); // Verify correct channel was selected
    }
    
    SECTION("Reading normalized values") {
        musin::hal::AnalogIn analog_in(28); // ADC2
        analog_in.init();
        
        // Set mock ADC value
        mock::adc_state.channel_values[2] = 4095; // Max value
        
        // Read and verify normalized value (should be 1.0)
        REQUIRE(analog_in.read() == Approx(1.0f));
    }
    
    SECTION("Reading voltage values") {
        musin::hal::AnalogIn analog_in(26); // ADC0
        analog_in.init();
        
        // Set mock ADC value
        mock::adc_state.channel_values[0] = 2048; // Half of max value
        
        // Read and verify voltage (should be half of reference voltage)
        REQUIRE(analog_in.read_voltage() == Approx(musin::hal::AnalogIn::ADC_REFERENCE_VOLTAGE / 2.0f));
    }
    
    SECTION("Reading without initialization") {
        musin::hal::AnalogIn analog_in(26); // ADC0
        
        // Reading without initialization should return 0
        REQUIRE(analog_in.read_raw() == 0);
        REQUIRE(analog_in.read() == 0.0f);
        REQUIRE(analog_in.read_voltage() == 0.0f);
    }
}

TEST_CASE_METHOD(AnalogInFixture, "AnalogInMux initialization and reading", "[analog_in]") {
    SECTION("Basic initialization") {
        std::array<std::uint32_t, 3> address_pins = {10, 11, 12};
        musin::hal::AnalogInMux8 mux(26, address_pins, 5); // ADC0, channel 5
        mux.init();
        
        // Verify ADC was initialized
        REQUIRE(mock::adc_state.initialized);
        
        // Verify GPIO pins were initialized
        REQUIRE(mock::gpio_states[26].initialized);
        REQUIRE(mock::gpio_states[26].direction == mock::GPIO_IN);
        
        for (auto pin : address_pins) {
            REQUIRE(mock::gpio_states[pin].initialized);
            REQUIRE(mock::gpio_states[pin].direction == mock::GPIO_OUT);
            REQUIRE(mock::gpio_states[pin].value == false); // Default to 0
        }
    }
    
    SECTION("Reading with address selection") {
        std::array<std::uint32_t, 3> address_pins = {10, 11, 12};
        musin::hal::AnalogInMux8 mux(27, address_pins, 5); // ADC1, channel 5 (binary 101)
        mux.init();
        
        // Set mock ADC value
        mock::adc_state.channel_values[1] = 1000;
        
        // Read and verify
        REQUIRE(mux.read_raw() == 1000);
        
        // Verify address pins were set correctly for channel 5 (binary 101)
        REQUIRE(mock::gpio_states[10].value == true);  // LSB = 1
        REQUIRE(mock::gpio_states[11].value == false); // Middle bit = 0
        REQUIRE(mock::gpio_states[12].value == true);  // MSB = 1
    }
    
    SECTION("Reading normalized and voltage values") {
        std::array<std::uint32_t, 3> address_pins = {10, 11, 12};
        musin::hal::AnalogInMux8 mux(28, address_pins, 2); // ADC2, channel 2
        mux.init();
        
        // Set mock ADC value
        mock::adc_state.channel_values[2] = 2048; // Half of max value
        
        // Read and verify normalized value
        REQUIRE(mux.read() == Approx(0.5f));
        
        // Read and verify voltage
        REQUIRE(mux.read_voltage() == Approx(musin::hal::AnalogIn::ADC_REFERENCE_VOLTAGE / 2.0f));
    }
    
    SECTION("Reading without initialization") {
        std::array<std::uint32_t, 3> address_pins = {10, 11, 12};
        musin::hal::AnalogInMux8 mux(26, address_pins, 0);
        
        // Reading without initialization should return 0
        REQUIRE(mux.read_raw() == 0);
        REQUIRE(mux.read() == 0.0f);
        REQUIRE(mux.read_voltage() == 0.0f);
    }
}

TEST_CASE_METHOD(AnalogInFixture, "pin_to_adc_channel function", "[analog_in]") {
    SECTION("Valid pin conversions") {
        REQUIRE(musin::hal::pin_to_adc_channel(26) == 0);
        REQUIRE(musin::hal::pin_to_adc_channel(27) == 1);
        REQUIRE(musin::hal::pin_to_adc_channel(28) == 2);
        REQUIRE(musin::hal::pin_to_adc_channel(29) == 3);
    }
}

TEST_CASE_METHOD(AnalogInFixture, "set_mux_address function", "[analog_in]") {
    SECTION("Setting address pins") {
        std::array<std::uint32_t, 3> address_pins = {10, 11, 12};
        
        // Initialize pins
        for (auto pin : address_pins) {
            gpio_init(pin);
            gpio_set_dir(pin, mock::GPIO_OUT);
        }
        
        // Test address 0 (binary 000)
        musin::hal::set_mux_address(address_pins, 0);
        REQUIRE(mock::gpio_states[10].value == false);
        REQUIRE(mock::gpio_states[11].value == false);
        REQUIRE(mock::gpio_states[12].value == false);
        
        // Test address 7 (binary 111)
        musin::hal::set_mux_address(address_pins, 7);
        REQUIRE(mock::gpio_states[10].value == true);
        REQUIRE(mock::gpio_states[11].value == true);
        REQUIRE(mock::gpio_states[12].value == true);
        
        // Test address 5 (binary 101)
        musin::hal::set_mux_address(address_pins, 5);
        REQUIRE(mock::gpio_states[10].value == true);
        REQUIRE(mock::gpio_states[11].value == false);
        REQUIRE(mock::gpio_states[12].value == true);
    }
}