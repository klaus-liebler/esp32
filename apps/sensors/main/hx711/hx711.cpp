#include "hx711.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "soc/rmt_reg.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"


static constexpr int RMT_CLK_DIV=16; // Grundtakt ist 5MHz
static constexpr uint32_t HALF_PERIOD = 10;
static constexpr uint32_t CLK_ITEM_VAL = HALF_PERIOD | 0x00008000 | (HALF_PERIOD<<16);
static constexpr uint32_t END_ITEM_VAL = 0x00008000;
static portMUX_TYPE rmt_spinlock = portMUX_INITIALIZER_UNLOCKED;
using namespace whs;

HX711::HX711(gpio_num_t dout, gpio_num_t pd_sck, rmt_channel_t tx_ch, rmt_channel_t rx_ch, InputAndGain gain) {

	this->GAIN=gain;
	this->PD_SCK=pd_sck;
	this->DOUT=dout;
	this->tx_ch = tx_ch;
	this->rx_ch=rx_ch;
	rmt_tx_config_t txspecificconfig={
		.loop_en = false,
		.carrier_freq_hz=0,
		.carrier_duty_percent=0,
		.carrier_level=RMT_CARRIER_LEVEL_LOW,
		.carrier_en = false,
		.idle_level = RMT_IDLE_LEVEL_LOW ,
		.idle_output_en = true
	};

	rmt_config_t rmt_tx;
	rmt_tx.rmt_mode = RMT_MODE_TX;
	rmt_tx.channel=tx_ch;
	rmt_tx.clk_div=RMT_CLK_DIV;
	rmt_tx.gpio_num=pd_sck;
	rmt_tx.mem_block_num=1;
	rmt_tx.tx_config=txspecificconfig;
	ESP_ERROR_CHECK(rmt_config(&rmt_tx));
	ESP_ERROR_CHECK(rmt_driver_install(rmt_tx.channel, 0, 0));

	for(int i=0;i<25;i++)
	{
		RMTMEM.chan[tx_ch].data32[i].val = CLK_ITEM_VAL;
	}
	for(int i=25;i<64;i++)
	{
		RMTMEM.chan[tx_ch].data32[i].val = END_ITEM_VAL;//clear out RAM
	}

	for(int i=0;i<64;i++)
	{
		RMTMEM.chan[rx_ch].data32[i].val = END_ITEM_VAL;
	}

	rmt_rx_config_t rxconfig={
		.filter_en = true,
		.filter_ticks_thresh = 3,
		.idle_threshold = UINT16_MAX
	};

	rmt_config_t rmt_rx;
	rmt_rx.channel=rx_ch;
	rmt_rx.gpio_num=dout;
	rmt_rx.mem_block_num=1;
	rmt_rx.clk_div=RMT_CLK_DIV;
	rmt_rx.rx_config=rxconfig;
	rmt_rx.rmt_mode = RMT_MODE_RX;
	ESP_ERROR_CHECK(rmt_config(&rmt_rx));
	ESP_ERROR_CHECK(rmt_driver_install(rmt_rx.channel, 0, 0));
}


static esp_err_t rmt_rx_tx_start(rmt_channel_t rx_channel, rmt_channel_t tx_channel)
{
    portENTER_CRITICAL(&rmt_spinlock);

    RMT.conf_ch[rx_channel].conf1.mem_wr_rst = 1;
	RMT.conf_ch[tx_channel].conf1.mem_rd_rst = 1;

    RMT.conf_ch[rx_channel].conf1.rx_en = 0;

    RMT.conf_ch[rx_channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
    RMT.conf_ch[tx_channel].conf1.mem_owner = RMT_MEM_OWNER_TX;

    RMT.conf_ch[tx_channel].conf1.tx_start = 1;
    RMT.conf_ch[rx_channel].conf1.rx_en = 1;

    portEXIT_CRITICAL(&rmt_spinlock);
    return ESP_OK;
}

static volatile rmt_item32_t* rmt_get_items(rmt_channel_t channel, int* count)
{
    int block_num = RMT.conf_ch[channel].conf0.mem_size;
    int item_block_len = block_num * RMT_MEM_ITEM_NUM;
    volatile rmt_item32_t* data = RMTMEM.chan[channel].data32;
    int idx;
    for(idx = 0; idx < item_block_len; idx++) {
        if(data[idx].duration0 == 0) {
            break;
        } else if(data[idx].duration1 == 0) {
            idx+= 1;
            break;
        }
    }
    *count=idx;
    return data;
}
int32_t HX711::read(InputAndGain nextInputAndGain)
{
	this->GAIN=nextInputAndGain;
	return read();
}

int32_t HX711::read()
{
	power_up();
	while (!is_ready()) {
		vTaskDelay(20 / portTICK_PERIOD_MS);
	}
	int clocks = 25+(int)this->GAIN;
	for(int i=25;i<clocks;i++)
	{
		RMTMEM.chan[tx_ch].data32[i].val = CLK_ITEM_VAL;
	}
	for(int i=clocks;i<28;i++)
	{
		RMTMEM.chan[tx_ch].data32[i].val = END_ITEM_VAL;
	}
	
	rmt_rx_tx_start(this->rx_ch, this->tx_ch);
	rmt_wait_tx_done(this->tx_ch, portMAX_DELAY);
	rmt_rx_stop(this->rx_ch);
	int count=0;
	volatile rmt_item32_t* rxItems = rmt_get_items(this->rx_ch, &count);
	if(!rxItems || count==0) {
		printf("No rxItems found");
		return UINT32_MAX;
	}
	printf("\n\nFound %d rxItems:\n", count);

	uint32_t val=0;

	uint32_t rxItemIndex = 0;
	//Erstes Element muss mit "Level Low" beginnen
	volatile rmt_item32_t* it = &rxItems[rxItemIndex];
	bool dur0Is0 = !it->level0;
	printf("dur0is0=%s\n", dur0Is0 ? "true" : "false");
	uint32_t currentHighStart=dur0Is0?it->duration0:0;
	uint32_t currentHighEnd=dur0Is0?currentHighStart+it->duration1:it->duration0;
	uint32_t t = HALF_PERIOD;
	printf("At %d: new 'it' with index %d. Data is {lev0: %d, dur0:%d, dur1:%d}\n", t, rxItemIndex, it->level0, it->duration0, it->duration1);
	printf("At %d: currentHighStart=%d, currentHighEnd=%d.\n" , t, currentHighStart, currentHighEnd);
	for(t=HALF_PERIOD;t<24*2*HALF_PERIOD;t+=2*HALF_PERIOD)
	{
		val=val<<1;
		if(dur0Is0)
		{
			
			if(t>currentHighEnd)
			{
				rxItemIndex++;
				if(rxItemIndex>=count)
				{
					currentHighEnd=0;
					currentHighEnd=UINT32_MAX;
				}
				else{
					it = &rxItems[rxItemIndex];
					currentHighStart = currentHighEnd+it->duration0;
					currentHighEnd=currentHighStart+it->duration1;
				}

			}
			if(t>=currentHighStart &&t<currentHighEnd)
			{
				val|=1;
			}
		}
		else //dur0is1  !!!
		{
			if(t>currentHighEnd+it->duration1)
			{
				
				currentHighStart = currentHighEnd+it->duration1; // duration1 from the current item
				rxItemIndex++;
				if(rxItemIndex>=count)
				{
					currentHighEnd=UINT32_MAX;
				}
				else{
					it = &rxItems[rxItemIndex];
					currentHighEnd = currentHighStart+it->duration0; // duration0 from the next item
				}
				printf("At %d: t>currentHighEnd+it->duration1\n", t);
				printf("At %d: new 'it' with index %d. Data is {lev0: %d, dur0:%d, dur1:%d}\n", t, rxItemIndex, it->level0, it->duration0, it->duration1);
				printf("At %d: currentHighStart=%d, currentHighEnd=%d.\n" , t, currentHighStart, currentHighEnd);
			}
			if(t<currentHighEnd) 
			{
				val|=1;
				printf("At %d: 1\n", t);
			}
			else{
				printf("At %d: 0\n", t);
			}
				
		}

	}
	// Replicate the most significant bit to pad out a 32-bit signed integer
	if (val & 0x800000) {
		val |= 0xFF000000;
	}

	return val;
}


bool HX711::is_ready() {
    return gpio_get_level(DOUT) == 0;
}



int32_t HX711::read_average(uint8_t times) {
	int64_t sum = 0;
	for (uint8_t i = 0; i < times; i++) {
		sum += read();
		vTaskDelay(20/portTICK_PERIOD_MS);
	}
	return sum / times;
}



void HX711::power_down() {
	rmt_set_idle_level(this->tx_ch, true, RMT_IDLE_LEVEL_LOW);
	vTaskDelay(20/portTICK_PERIOD_MS);
	rmt_set_idle_level(this->tx_ch, true, RMT_IDLE_LEVEL_HIGH);
}

void HX711::power_up() {
	rmt_set_idle_level(this->tx_ch, true, RMT_IDLE_LEVEL_LOW);
}
