#pragma once
#include "driver/gpio.h"
#include <esp_types.h>
#include "driver/rmt.h"
namespace whs{

enum InputAndGain {A128=0, B32=1, A64=2};
class HX711
{
	private:
		gpio_num_t PD_SCK;	// Power Down and Serial Clock Input Pin
		gpio_num_t DOUT;		// Serial Data Output Pin
		rmt_channel_t tx_ch;
		rmt_channel_t rx_ch;
		InputAndGain GAIN;		// amplification factor
		long OFFSET = 0;	// used for tare weight
		float SCALE = 1;	// used to return weight in grams, kg, ounces, whatever

	public:
		// define clock and data pin, channel, and gain factor
		// channel selection is made by passing the appropriate gain: 128 or 64 for channel A, 32 for channel B
		// gain: 128 or 64 for channel A; channel B works with 32 gain factor only
		HX711(gpio_num_t dout, gpio_num_t pd_sck, rmt_channel_t tx_ch, rmt_channel_t rx_ch, InputAndGain initialInputAndGain);

		// waits for the chip to be ready and returns a reading
		int32_t read();
		int32_t read(InputAndGain nextInputAndGain);

		// check if HX711 is ready
		// from the datasheet: When output data is not ready for retrieval, digital output pin DOUT is high. Serial clock
		// input PD_SCK should be low. When DOUT goes to low, it indicates data is ready for retrieval.
		bool is_ready();

		// returns an average reading; times = how many times to read
		int32_t read_average(uint8_t times = 10);

		// puts the chip into power down mode
		void power_down();

		// wakes up the chip after power down mode
		void power_up();
};
}


