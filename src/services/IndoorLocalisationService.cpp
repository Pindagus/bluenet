/*
 * IndoorLocalisationService.cpp
 *
 *  Created on: Oct 21, 2014
 *      Author: dominik
 */

#include <cmath> // try not to use this!
#include <cstdio>

#include <services/IndoorLocalisationService.h>
#include <common/config.h>
#include <common/boards.h>
#include <drivers/nrf_adc.h>

using namespace BLEpp;

IndoorLocalizationService::IndoorLocalizationService(Nrf51822BluetoothStack& _stack) :
		_rssiCharac(NULL), _intChar(NULL), _intChar2(NULL), _peripheralCharac(NULL),
		_personalThresholdLevel(0) {
	this->_stack = &_stack;

	setUUID(UUID(INDOORLOCALISATION_UUID));
	//setUUID(UUID(0x3800)); // there is no BLE_UUID for indoor localization (yet)

	// we have to figure out why this goes wrong
	setName(std::string("IndoorLocalizationService"));
}

void IndoorLocalizationService::AddSpecificCharacteristics() {
	AddRssiCharacteristic();
//	AddNumberCharacteristic();
//	AddNumber2Characteristic();
//	AddVoltageCurveCharacteristic();
	AddScanControlCharacteristic();
	AddPeripheralListCharacteristic();
	AddPersonalThresholdCharacteristic();
}

void IndoorLocalizationService::AddRssiCharacteristic() {
	_rssiCharac = new CharacteristicT<int8_t>();
	//.setUUID(UUID(service.getUUID(), 0x124))
	_rssiCharac->setUUID(UUID(getUUID(), 0x2201)); // there is no BLE_UUID for rssi level(?)
	_rssiCharac->setName(std::string("Received signal level"));
	_rssiCharac->setDefaultValue(1);
	_rssiCharac->setNotifies(true);

	addCharacteristic(_rssiCharac);
}

void IndoorLocalizationService::AddNumberCharacteristic() {
	// create a characteristic of type uint8_t (unsigned one byte integer).
	// this characteristic is by default read-only (for the user)
	// note that in the next characteristic this variable intchar is set!
	log(DEBUG, "create characteristic to read a number for debugging");
	//Characteristic<uint8_t>&
	_intChar = createCharacteristicRef<uint8_t>();
	(*_intChar)
		.setUUID(UUID(getUUID(), 0x125))  // based off the uuid of the service.
		.setDefaultValue(66)
		.setName("number");
}

void IndoorLocalizationService::AddNumber2Characteristic() {
	_intChar2 = createCharacteristicRef<uint64_t>();
	(*_intChar2)
		.setUUID(UUID(getUUID(), 0x121))  // based off the uuid of the service.
		.setDefaultValue(66)
		.setName("number2");
}

void IndoorLocalizationService::AddVoltageCurveCharacteristic() {
	log(DEBUG, "create characteristic to read voltage curve");
	createCharacteristic<uint8_t>()
		.setUUID(UUID(getUUID(), 0x124))
		.setName("led")
		.setDefaultValue(255)
		.setWritable(true)
		.onWrite([&](const uint8_t& value) -> void {
			log(INFO, "Received message: %d", value);

/*
			uint64_t rms_sum = 0;
			uint32_t voltage_min = 0xffffffff;
			uint32_t voltage_max = 0;
			// start reading adc
			uint32_t voltage;
			uint32_t samples = 100000;
			//uint32_t subsample = samples / curve_size;
*/
			//log(INFO, "Stop advertising");
			//_stack->stopAdvertising();

			log(INFO, "Start ADC");
			nrf_adc_start();
			// replace by timer!
			while (!adc_result.full()) {
				nrf_delay_ms(100);
			}
			log(INFO, "Number of results: %u", adc_result.count());
			log(INFO, "Stop ADC converter");
			nrf_adc_stop();
/*
			for (uint32_t i=0; i<samples; ++i) {

				nrf_adc_read(PIN_ADC, &voltage);

				rms_sum += voltage*voltage;
				if (voltage < voltage_min)
					voltage_min = voltage;
				if (voltage > voltage_max)
					voltage_max = voltage;
				//if (!(i % subsample)) 
				if (i < curve_size)
					curve[i] = voltage;
			}
			uint32_t rms = sqrt(rms_sum/(100*1000));

			// 8a max --> 0.96v max --> voltage value is max 960000, which is smaller than 2^32

			// measured voltage goes from 0-3.6v(due to 1/3 multiplier), measured as 0-255(8bit) or 0-1024(10bit)
//			voltage = voltage*1000*1200*3/255; // nv   8 bit
			voltage     = voltage    *1000*1200*3/1024; // nv   10 bit
			voltage_min = voltage_min*1000*1200*3/1024;
			voltage_max = voltage_max*1000*1200*3/1024;
			rms         = rms        *1000*1200*3/1024;

			uint16_t current = rms / SHUNT_VALUE; // ma

			log(DEBUG, "voltage(nV): last=%lu", voltage);
		       	//rms=%lu min=%lu max=%lu current=%i mA", voltage, rms, voltage_min, voltage_max, current);
*/
			
			char curve_text[128];
			//curve_size = adc_result.count();
			//for (int i = 0; i < adc_result.count()
			int i = 0;
			while (!adc_result.empty()) { 
//			for (uint32_t i = 0; i < curve_size; ++i) {
				sprintf(curve_text, "%lu, ", adc_result.pop());
			//	sprintf(curve_text, "%lu, ", curve[i]);
				if (!(i++ % 10)) sprintf(curve_text, "\r\n");
				write(curve_text);
				curve_text[0]='\0';
			}
			write("\r\n");	
			//_stack->startAdvertising(); // segfault
/*
			uint64_t result = voltage_min;
			result <<= 32;
			result |= voltage_max;
#ifdef NUMBER_CHARAC
			*intchar = result;
			result = rms;
			result <<= 32;
			result |= current;
			*_intChar2 = result;
#endif
*/
#ifdef BINARY_LED
			bin_counter++;
			if (bin_counter % 2) {
				NRF51_GPIO_OUTSET = 1 << PIN_LED; // PIN HIGH, LED GOES ON
			} else {
				NRF51_GPIO_OUTCLR = 1 << PIN_LED; // PIN LOW, LED GOES OFF
			}
#endif

			log(DEBUG, "Successfully written");
		});
}

void IndoorLocalizationService::AddScanControlCharacteristic() {
	// set scanning option
	log(DEBUG, "create characteristic to stop/start scan");
	createCharacteristic<uint8_t>()
		.setUUID(UUID(getUUID(), 0x123))
		.setName("scan")
		.setDefaultValue(255)
		.setWritable(true)
		.onWrite([&](const uint8_t & value) -> void {
			if(value) {
				log(INFO,"crown: start scanning");
				if (!_stack->isScanning()) {
//					scanResult.init(10);
					_stack->startScanning();
				}
			} else {
				log(INFO,"crown: stop scanning");
				if (_stack->isScanning()) {
					_stack->stopScanning();
					*_peripheralCharac = scanResult;
					scanResult.print();
				}
			}
		});
}

void IndoorLocalizationService::AddPeripheralListCharacteristic() {
	// get scan result
	log(DEBUG, "create characteristic to list found peripherals");

	_peripheralCharac = createCharacteristicRef<ScanResult>();
	_peripheralCharac->setUUID(UUID(getUUID(), 0x120));
	_peripheralCharac->setName("Peripherals");
	_peripheralCharac->setWritable(false);
	_peripheralCharac->setNotifies(true);
}

void IndoorLocalizationService::AddPersonalThresholdCharacteristic() {
	// set threshold level
	log(DEBUG, "create characteristic to write personal threshold level");
	createCharacteristic<uint8_t>()
		.setUUID(UUID(getUUID(), 0x122))
		.setName("threshold")
		.setDefaultValue(0)
		.setWritable(true)
		.onWrite([&](const uint8_t & value) -> void {
			nrf_pwm_set_value(0, value);
//			nrf_pwm_set_value(1, value);
//			nrf_pwm_set_value(2, value);
			log(INFO, "set personal_threshold_value to %i", value);
			_personalThresholdLevel = value;
		});
}

IndoorLocalizationService& IndoorLocalizationService::createService(Nrf51822BluetoothStack& _stack) {
	IndoorLocalizationService* svc = new IndoorLocalizationService(_stack);
	_stack.addService(svc);
	svc->AddSpecificCharacteristics();
	return *svc;
}

void IndoorLocalizationService::on_ble_event(ble_evt_t * p_ble_evt) {
	Service::on_ble_event(p_ble_evt);
	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED: {
		sd_ble_gap_rssi_start(p_ble_evt->evt.gap_evt.conn_handle);
		break;
	}
	case BLE_GAP_EVT_DISCONNECTED: {
		sd_ble_gap_rssi_stop(p_ble_evt->evt.gap_evt.conn_handle);
		break;
	}
	case BLE_GAP_EVT_RSSI_CHANGED: {
		onRSSIChanged(p_ble_evt->evt.gap_evt.params.rssi_changed.rssi);
		break;
	}
	default: {
	}
	}
}

void IndoorLocalizationService::onRSSIChanged(int8_t rssi) {

	// set LED here
	int sine_index = (rssi - 170) * 2;
	if (sine_index < 0) sine_index = 0;
	if (sine_index > 100) sine_index = 100;
	//			__asm("BKPT");
	//			int sine_index = (rssi % 10) *10;
	nrf_pwm_set_value(0, sin_table[sine_index]);
	nrf_pwm_set_value(1, sin_table[(sine_index + 33) % 100]);
	nrf_pwm_set_value(2, sin_table[(sine_index + 66) % 100]);
	//			counter = (counter + 1) % 100;

	// Add a delay to control the speed of the sine wave
	nrf_delay_us(8000);

	setRSSILevel(rssi);
}

void IndoorLocalizationService::setRSSILevel(int8_t RSSILevel) {
	if (_rssiCharac) {
		(*_rssiCharac) = RSSILevel;
	}
}

void IndoorLocalizationService::setRSSILevelHandler(func_t func) {
	_rssiHandler = func;
}


