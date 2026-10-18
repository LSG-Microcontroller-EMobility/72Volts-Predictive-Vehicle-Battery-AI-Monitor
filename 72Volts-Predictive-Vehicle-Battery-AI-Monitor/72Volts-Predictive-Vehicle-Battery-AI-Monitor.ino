/*
 Name:		72Volts-Predictive-Vehicle-Battery-AI-Monitor.ino
 Created:	3/8/2020 1:53:08 AM
 Author:	luigi.santagada
*/
#include "SoftwareSerial.h"
#include <SD.h>
#include <SPI.h>
#include <string.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>
#include <stdlib.h>   // strtof
#include <stdbool.h>  // bool
#include <stddef.h>   // NULL
#ifndef _DEBUG_FOR_SERIAL
#define _DEBUG_FOR_SERIAL 1
#endif
#ifndef _IS_ON_VOLTAGE_TEST
#define _IS_ON_VOLTAGE_TEST 0
#endif
#ifndef _IS_ON_AI_TEST
#define _IS_ON_AI_TEST 0
#endif
#define AUDIO_DISLIVELLO_BATTERIE 1
#define AUDIO_TRACCIA_ERRATA 2
#define AUDIO_NUMERO_ERRATO 3
#define AUDIO_PROBLEMA_SCHEDA_MEMORIA 4
#define AUDIO_DATI_SUFFICIENTI 5
#define AUDIO_SISTEMA_INIZIALIZZATO 6
#define AUDIO_ID_MESSAGE_WRONG 7
#define AUDIO_SISTEMA_INSTABILE 8
#define AUDIO_AQUISIZIONE_DATI 9
#define AUDIO_SCHEDA_MEM_PIENA 10
#define AUDIO_DELETE_FILES_30_SEC 11
#define AUDIO_DELETE_FILES_10_SEC 12
#define AUDIO_ALL_FILES_DELETED 13	
#define AUDIO_INIZIO_STRESS_TEST 14	
const uint8_t _numberOfBattery = 6;
const uint8_t _pin_selectorMultiPlex0 = 4;
const uint8_t _pin_selectorMultiPlex1 = 5;
const uint8_t _pin_selectorMultiPlex2 = 6;
const uint8_t _pin_selectorMultiPlex3 = 7;
const uint8_t _pin_rx = 3;
const uint8_t _pin_interrupt_to_attiny85 = 9;
const uint8_t _pin_dfMiniPlayer_rx = A1;
const uint8_t _pin_dfMiniPlayer_tx = A2;
const uint8_t _pin_dfMiniPlayer_volume = A3;
const uint8_t _pin_maxBatteryVoltageDifference = A4;
uint8_t total_takeovers = 0;
const uint8_t max_total_takeovers = 2;
const uint8_t max_AI_error_percentage = 20;
const uint8_t demultiplexer_position_start = 0;
const uint8_t csv_battery_text_layout_capacity = 12;
//-----------------------    ATTENZIONE PIN ASSEGNATI a scheda SD file excel !!!!!!!   -------------------------------
// Pin 11 MOSI	Pin 12 MISO		Pin 13 SCK
//const float deltaVoltage[_numberOfBattery] = { 0.00, 0.00 , 0.00, 0.00, 0.00 ,0.00 };
float storedBatteryValues[_numberOfBattery] = { 0.00, 0.00 , 0.00, 0.00, 0.00 ,0.00 };
float stored_ampere = 0.00;
float stored_watts = 0.00;
uint8_t _demultiplexerPosition = demultiplexer_position_start;
uint8_t fileNumber = 0;
uint8_t numeroDisallineamenti = 0;
char _idMessage[1] = { 'x' };
float batteryMaxLevel = 0.00f;
float batteryMinLevel = 0.00f;
const uint8_t max_files_numbers = 9;  //MAX 9
uint8_t ii = 0;
bool _is_card_writing_disable = false;
bool _is_DPlayer_disable = false;
char fileName[15] = {};
const uint8_t numberOf_X = 2;
const uint8_t numberOf_Y = 6;
const float leaky_relu_alpha = 0.01f;
float x[numberOf_X] = { 0.00 };
float y[numberOf_Y] = { 0.00f };
float leaky_relu(float value) {
	return (value > 0.0f) ? value : leaky_relu_alpha * value;
}
void run_neural_network_forward_pass() {
	int addr = 0;
	float Zk = 0.00f;
	float Zj = 0.00f;
	float data_from_eeprom = 0.00f;
	const uint8_t numberOf_H = 25;
	float h[numberOf_H] = { 0.00 };
	for (int k = 0; k < (numberOf_H); k++) {
		Zk = 0.00f;
		for (int i = 0; i < numberOf_X; i++) {
			EEPROM.get(addr, data_from_eeprom);
			Zk += (data_from_eeprom * x[i]);
			addr += sizeof(float);
		}
		//insert X bias
		EEPROM.get(addr, data_from_eeprom);
		Zk += data_from_eeprom;
		h[k] = leaky_relu(Zk);
		addr += sizeof(float);
	}
	for (int j = 0; j < numberOf_Y; j++) {
		Zj = 0.00f;
		for (int k = 0; k < numberOf_H; k++) {
			EEPROM.get(addr, data_from_eeprom);
			Zj += (data_from_eeprom * h[k]);
			addr += sizeof(float);
		}
		EEPROM.get(addr, data_from_eeprom);
		//insert H bias
		Zj += data_from_eeprom;
		y[j] = Zj;
		addr += sizeof(float);
	}
}
float mean_squared_error(const float* arr1, const float* arr2, int size) {
	float sum = 0.0f;
	for (int i = 0; i < size; ++i) {
		float diff = arr1[i] - arr2[i];
		sum += (diff * diff);  // quadrato della differenza
		//Serial.print(F("diff: ")); Serial.println(diff); 
	}
	// MSE = (1 / N) * Σ (diff^2)
	return sum / size;
}
float calculate_arithmetic_mean(const float* data, int size) {
	float sum = 0.0f;
	for (int i = 0; i < size; ++i) {
		sum += data[i];
	}
	return sum / size;
}
uint16_t calculate_normalized_rmse_percentage(float mse, float reference_mean) {
	// Calcola il Root Mean Squared Error (RMSE)
	float rms = sqrtf(mse);
	// Calcola la percentuale (tronca i decimali):
	float pct = (rms / reference_mean) * 100.0f;
	return (uint16_t)pct;
}
void setup() {
	analogReference(EXTERNAL);
	trigger_all_attiny85_measurements();
#if !_IS_ON_VOLTAGE_TEST
	delay(5000);
#endif // !_IS_ON_VOLTAGE_TEST
	pinMode(_pin_interrupt_to_attiny85, OUTPUT);
	pinMode(_pin_selectorMultiPlex0, OUTPUT);
	pinMode(_pin_selectorMultiPlex1, OUTPUT);
	pinMode(_pin_selectorMultiPlex2, OUTPUT);
	pinMode(_pin_selectorMultiPlex3, OUTPUT);
	digitalWrite(_pin_selectorMultiPlex0, LOW);
	digitalWrite(_pin_selectorMultiPlex1, LOW);
	digitalWrite(_pin_selectorMultiPlex2, LOW);
	digitalWrite(_pin_selectorMultiPlex3, LOW);
	digitalWrite(_pin_interrupt_to_attiny85, LOW);
#if _DEBUG_FOR_SERIAL
	Serial.begin(9600);
#endif // _DEBUG_FOR_SERIAL
	initialize_sd_card_data_file();
#if _DEBUG_FOR_SERIAL
	Serial.println(F("rest."));
#endif // _DEBUG_FOR_SERIAL
	play_audio_message(AUDIO_SISTEMA_INIZIALIZZATO);
}
void initialize_sd_card_data_file() {
	if (_is_card_writing_disable) return;
	if (SD.begin()) {
		bool exit = false;
		uint8_t cicle = 0;
		//Serial.println(F("card ready"));
		while (cicle < max_files_numbers && !exit) {
			strcpy(fileName, "batt");
			fileName[4] = (char)(cicle + 48);
			strcat(fileName, ".csv");
			fileName[9] = '\0';
#if _DEBUG_FOR_SERIAL
			Serial.println(fileName);
#endif // _DEBUG_FOR_SERIAL
			if (SD.exists(fileName)) {
#if _DEBUG_FOR_SERIAL
				Serial.println(F("F.Exist"));
#endif // _DEBUG_FOR_SERIAL
				cicle++;
				if (cicle == max_files_numbers) {
					play_audio_message(AUDIO_SCHEDA_MEM_PIENA);
					play_audio_message(AUDIO_DELETE_FILES_30_SEC);
					delay(20);
					play_audio_message(AUDIO_DELETE_FILES_10_SEC);
					delay(10);
					for (uint8_t i = 0; i < max_files_numbers; i++) {
						strcpy(fileName, "batt");
						fileName[4] = (char)(i + 48);
						strcat(fileName, ".csv");
						fileName[9] = '\0';
						SD.remove(fileName);
					}
					play_audio_message(AUDIO_ALL_FILES_DELETED);
					cicle = 0;
				}
			}
			else {
				exit = true;
			}
		}
		char headersText[37] = "IDMessage;Battery;Value;W/h;amps";

		append_line_to_sd_card(headersText);

	}
	else {
		//buzzer_sensor_activity(5, 400, 1000, 500);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("SD failed"));
#endif // _DEBUG_FOR_SERIAL
		play_audio_message(AUDIO_PROBLEMA_SCHEDA_MEMORIA);
		while (true) {};
	}
}
void loop() {
#if _DEBUG_FOR_SERIAL
	Serial.print(F("mem :")); Serial.println(get_free_sram_bytes());
#endif // _DEBUG_FOR_SERIAL
	//#if _IS_ON_AI_TEST
	//	are_predicted_battery_values_acceptable();
	//	delay(2000);
	//	return;
	//#endif // _IS_ON_AI_TEST
		/*play_audio_message(AUDIO_INIZIO_STRESS_TEST);

		Serial.println("inizio del test");

		return;*/
		////volume test
		//Serial.print("volume : "); Serial.println(analogRead(A3));
		////volume test
		////percentage test
		//Serial.print("percentage : "); Serial.println(analogRead(A4));
		////percentage test
		//delay(500);
		//return;
		//trigger_all_attiny85_measurements();
		//delay(1000);
		//return;
#if _IS_ON_VOLTAGE_TEST
	_demultiplexerPosition = 0;
#endif // _IS_ON_VOLTAGE_TEST
	select_multiplexer_channel(_demultiplexerPosition);
	char response[6] = {};
	read_battery_response_from_serial(&response[0]);
#if _IS_ON_VOLTAGE_TEST
#if _DEBUG_FOR_SERIAL
	Serial.print(F("#")); Serial.print(response); Serial.println(F("#"));
#endif // _DEBUG_FOR_SERIAL
	trigger_all_attiny85_measurements();
	return;
#endif // _IS_ON_VOLTAGE_TEST
#if _DEBUG_FOR_SERIAL
	Serial.print(F("#")); Serial.print(response); Serial.println(F("#"));
#endif // _DEBUG_FOR_SERIAL
	uint8_t max_attempts = 0;
	//Attempts if number transformation fails.
	while ((!is_valid_battery_voltage_response(response) || response[0] == '.') && max_attempts < 5) {
		read_battery_response_from_serial(&response[0]);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("n.w"));
#endif // _DEBUG_FOR_SERIAL
		max_attempts++;
	}
	//if number transformation was failed
	if (!is_valid_battery_voltage_response(response)) {
		play_audio_message(AUDIO_NUMERO_ERRATO);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("not.n"));
#endif // _DEBUG_FOR_SERIAL
		_demultiplexerPosition = demultiplexer_position_start;
		return;
	}
	if (_idMessage[0] != 'x') {
		if (_idMessage[0] != response[4]) {
			play_audio_message(AUDIO_ID_MESSAGE_WRONG);
#if _DEBUG_FOR_SERIAL
			Serial.println(F("id.pr"));
#endif // _DEBUG_FOR_SERIAL
			_demultiplexerPosition = demultiplexer_position_start;
			_idMessage[0] = 'x';
			return;
		}
	}
	else {
		_idMessage[0] = response[4];
	}
	char csv_battery_text_layout[csv_battery_text_layout_capacity] = {};
	build_battery_csv_record(csv_battery_text_layout, response);
	if (is_battery_csv_record_invalid(csv_battery_text_layout, csv_battery_text_layout_capacity)) {
		_demultiplexerPosition = demultiplexer_position_start;
		play_audio_message(AUDIO_TRACCIA_ERRATA);
		return;
	}
	store_current_battery_voltage(response);
	append_line_to_sd_card(csv_battery_text_layout);
	if (_demultiplexerPosition == (_numberOfBattery - 1)) {
#if _DEBUG_FOR_SERIAL
		print_stored_battery_voltages();
#endif // _DEBUG_FOR_SERIAL
		check_battery_balance();
		select_multiplexer_channel(6);
		read_power_measurements_from_serial();
		write_power_measurements_to_sd_card();
		_demultiplexerPosition = demultiplexer_position_start;
		_idMessage[0] = 'x';
		total_takeovers++;
		if (total_takeovers == max_total_takeovers) {
			total_takeovers = 0;
			//for simulation
			//stored_ampere = 39.00;
			if (!are_predicted_battery_values_acceptable() && (stored_ampere > 15.00f)) {
				play_audio_message(AUDIO_SISTEMA_INSTABILE);
			}
			else {
				play_audio_message(AUDIO_AQUISIZIONE_DATI);
			}
		}
		for (uint8_t i = 0; i < _numberOfBattery; i++) {
			storedBatteryValues[i] = 0.00;
		}
		trigger_all_attiny85_measurements();
	}
	else {
		_demultiplexerPosition++;
	}
}
bool is_battery_csv_record_invalid(const char* csv_text_layout, uint8_t csv_text_layout_capacity) {
	if (csv_text_layout == NULL || csv_text_layout_capacity < 9) return true;
	if (csv_text_layout[0] < '0' || csv_text_layout[0] > '9') return true;
	if (csv_text_layout[1] != ';' || csv_text_layout[2] != 'B') return true;
	if (csv_text_layout[3] < '0' || csv_text_layout[3] >= ('0' + _numberOfBattery)) return true;
	if (csv_text_layout[4] != ';') return true;

	uint8_t value_index = 5;
	bool has_digit = false;
	bool has_decimal_point = false;
	while (value_index < csv_text_layout_capacity && csv_text_layout[value_index] != ';') {
		const char current_character = csv_text_layout[value_index];
		if (current_character >= '0' && current_character <= '9') {
			has_digit = true;
		}
		else if (current_character == '.' && !has_decimal_point) {
			has_decimal_point = true;
		}
		else {
			return true;
		}
		value_index++;
	}

	if (!has_digit || value_index + 2 >= csv_text_layout_capacity) return true;
	return csv_text_layout[value_index] != ';' || csv_text_layout[value_index + 1] != ';' || csv_text_layout[value_index + 2] != '\0';
}
void store_current_battery_voltage(char response[6]) {
	float number = parse_battery_voltage(response);
	number = number; /*+deltaVoltage[_demultiplexerPosition];*/
	storedBatteryValues[_demultiplexerPosition] = number;
}
#if _DEBUG_FOR_SERIAL
void print_stored_battery_voltages() {
	for (uint8_t i = 0; i < _numberOfBattery; i++) {
		Serial.println(storedBatteryValues[i]);
	}
}
#endif // _DEBUG_FOR_SERIAL
void check_battery_balance() {
	update_max_battery_voltage(storedBatteryValues[0]);
	update_min_battery_voltage(storedBatteryValues[0]);
#if _DEBUG_FOR_SERIAL
	Serial.print(F("Mx.V:")); Serial.println(batteryMaxLevel);
#endif // _DEBUG_FOR_SERIAL
#if _DEBUG_FOR_SERIAL
	Serial.print(F("Min.V:")); Serial.println(batteryMinLevel);
#endif // _DEBUG_FOR_SERIAL
	if (are_batteries_unbalanced()) {
#if _DEBUG_FOR_SERIAL
		Serial.println(F("Dis.bat."));
#endif // _DEBUG_FOR_SERIAL
		play_audio_message(AUDIO_DISLIVELLO_BATTERIE);
		//buzzer_sensor_activity(5, 2500, 80, 200);
	}
}
bool is_valid_battery_voltage_response(const char* s) {
	if (s == NULL)
		return false;               // 1) puntatore nullo

	// 2) non accettiamo stringhe vuote né solo “.”
	if (*s == '\0' || (*s == '.' && s[1] == '\0'))
		return false;

	// 3) validazione manuale: cifre e al più un punto decimale
	const char* p = s;
	bool has_dot = false;
	bool has_digit = false;

	for (; *p; ++p) {
		if (*p >= '0' && *p <= '9') {
			has_digit = true;
		}
		else if (*p == '.') {
			if (has_dot)
				return false;
			has_dot = true;
		}
		else {
			return false;
		}
	}
	if (!has_digit)
		return false;

	// 4) parsing con strtod (double è float su AVR)
	char* end;
	float val = (float)strtod(s, &end);

	// 5) strtod deve aver consumato tutta la stringa
	if (end == s || *end != '\0')
		return false;

	// 6) intervallo aperto (0.00, 4.50)
	if (!(val > 0.0f && val < 4.5f))
		return false;

	return true;
}
bool are_batteries_unbalanced() {
	//https://www.desmos.com/calculator/wsfbcw9ffn
	//See math site for percentage calculate.
	//float maxPercentageForAlarm = analogRead(_pin_maxBatteryVoltageDifference) / (1024.00 / 15.00 /*<--max percentage*/);
	float x = 0.00f;
	for (int i = 0; i < _numberOfBattery; i++) {
		x = x + storedBatteryValues[i];
	}
	x = x / _numberOfBattery;
	float maxPercentageForAlarm = -(8.60f * x) + 32.15f;
	float percentageValue = 100 - ((batteryMinLevel / batteryMaxLevel) * 100);
#if _DEBUG_FOR_SERIAL
	Serial.print(F("% value : ")); Serial.print(percentageValue); Serial.println(F("%"));
	Serial.print(F("% max : ")); Serial.print(maxPercentageForAlarm); Serial.println(F("%"));
#endif // _DEBUG_FOR_SERIAL
	if (percentageValue > maxPercentageForAlarm) {
		return true;
	}
	return false;
}
float parse_battery_voltage(char* response) {
	return atof(response);
}
void read_battery_response_from_serial(char* response) {
	if (response == NULL) return;
	response[0] = '\0';
	SoftwareSerial softwareSerial(_pin_rx, 99);
	softwareSerial.begin(600);
	while (!softwareSerial);
	const uint8_t expected_payload_length = 5;
	const unsigned long frame_timeout_ms = 1200UL;
	const unsigned long start_time_ms = millis();
	bool is_frame_synchronized = false;
	bool is_payload_too_long = false;
	uint8_t received_length = 0;
	while ((millis() - start_time_ms) < frame_timeout_ms) {
		const int received_byte = softwareSerial.read();
		if (received_byte < 0) continue;
		if (received_byte == '*') {
			if (is_frame_synchronized && !is_payload_too_long && received_length == expected_payload_length) {
				response[received_length] = '\0';
				return;
			}
			is_frame_synchronized = true;
			is_payload_too_long = false;
			received_length = 0;
			continue;
		}
		if (!is_frame_synchronized) continue;
		if (received_length < expected_payload_length) {
			response[received_length] = (char)received_byte;
			received_length++;
		}
		else {
			is_payload_too_long = true;
		}
	}
	response[0] = '\0';
}
void read_power_measurements_from_serial() {
	stored_ampere = 0.00f;
	stored_watts = 0.00f;
	SoftwareSerial softwareSerial(_pin_rx, 99);
	softwareSerial.begin(600);
	// Frame del Watt-Meter: '*' + ampere + byte NUL + Wh accumulati.
	// Il successivo '*' chiude il messaggio; i numeri hanno due decimali.
	char received_text[32] = {};
	const uint8_t separator_not_found = sizeof(received_text);
	const unsigned long reception_timeout_ms = 2000UL;
	const unsigned long reception_start_ms = millis();
	uint8_t received_text_length = 0;
	uint8_t separator_position = separator_not_found;
	bool has_frame_start = false;
	bool is_frame_invalid = false;
	while ((millis() - reception_start_ms) < reception_timeout_ms) {
		if (softwareSerial.overflow()) {
			// Il buffer seriale ha perso byte: svuotalo e attendi un nuovo '*'.
			const int buffered_byte_count = softwareSerial.available();
			for (int byte_index = 0; byte_index < buffered_byte_count; byte_index++) softwareSerial.read();
			has_frame_start = false;
			received_text_length = 0;
			separator_position = separator_not_found;
			is_frame_invalid = false;
		}
		const int received_byte = softwareSerial.read();
		if (received_byte < 0) continue;
		// '*' chiude il messaggio precedente e apre il successivo.
		if (received_byte == '*') {
			if (has_frame_start && !is_frame_invalid &&
				separator_position > 0 && separator_position < received_text_length - 1) {
				received_text[received_text_length] = '\0';
				bool are_numbers_valid = true;
				for (uint8_t measurement_index = 0; measurement_index < 2; measurement_index++) {
					const char* number_text = received_text + (measurement_index == 0 ? 0 : separator_position + 1);
					bool has_integer_digit = false;
					bool has_decimal_point = false;
					uint8_t decimal_digit_count = 0;
					for (const char* number_character = number_text; *number_character != '\0'; number_character++) {
						if (*number_character >= '0' && *number_character <= '9') {
							if (has_decimal_point) decimal_digit_count++;
							else has_integer_digit = true;
						}
						else if (*number_character == '.' && has_integer_digit && !has_decimal_point) {
							has_decimal_point = true;
						}
						else {
							are_numbers_valid = false;
							break;
						}
					}
					if (!has_integer_digit || !has_decimal_point || decimal_digit_count != 2) {
						are_numbers_valid = false;
					}
				}
				if (are_numbers_valid) {
					char* ampere_text_end;
					char* watt_hours_text_end;
					const float received_ampere = (float)strtod(received_text, &ampere_text_end);
					const float received_watt_hours = (float)strtod(received_text + separator_position + 1, &watt_hours_text_end);
					if (ampere_text_end == received_text + separator_position && watt_hours_text_end == received_text + received_text_length) {
						stored_ampere = received_ampere;
						// Il nome storico stored_watts contiene energia in Wh.
						stored_watts = received_watt_hours;
						break;
					}
				}
			}
			has_frame_start = true;
			is_frame_invalid = false;
			received_text_length = 0;
			separator_position = separator_not_found;
			continue;
		}
		if (!has_frame_start) continue;
		if (received_text_length >= sizeof(received_text) - 1) {
			is_frame_invalid = true;
			continue;
		}
		if (received_byte == '\0') {
			if (separator_position != separator_not_found) is_frame_invalid = true;
			else separator_position = received_text_length;
		}
		received_text[received_text_length++] = (char)received_byte;
	}
#if _DEBUG_FOR_SERIAL
	Serial.print(F("Ampere :")); Serial.println(stored_ampere);
	Serial.print(F("Watts/h :")); Serial.println(stored_watts);
#endif // _DEBUG_FOR_SERIAL
}
void select_multiplexer_channel(int channel) {
	uint8_t controlPin[4] = { _pin_selectorMultiPlex0, _pin_selectorMultiPlex1, _pin_selectorMultiPlex2, _pin_selectorMultiPlex3 };
	const uint8_t muxChannel[7][4] = {
		{0, 0, 0, 0}, // channel 0
		{1, 0, 0, 0}, // channel 1
		{0, 1, 0, 0}, // channel 2
		{1, 1, 0, 0}, // channel 3
		{0, 0, 1, 0}, // channel 4
		{1, 0, 1, 0}, // channel 5
		{0, 1, 1, 0}, // channel 6
		//{1, 1, 1, 0}, // channel 7
		//{0, 0, 0, 1}, // channel 8
		//{1, 0, 0, 1}, // channel 9
		//{0, 1, 0, 1}, // channel 10
		//{1, 1, 0, 1}, // channel 11
		//{0, 0, 1, 1}, // channel 12
		//{1, 0, 1, 1}, // channel 13
		//{0, 1, 1, 1}, // channel 14
		//{1, 1, 1, 1}  // channel 15
	};
	// loop through the 4 sig
	for (int i = 0; i < 4; i++) {
		digitalWrite(controlPin[i], muxChannel[channel][i]);
	}
}
void build_battery_csv_record(char* csvTextLayOut, char response[6]) {
	const char* idBattery[_numberOfBattery] = { "B0", "B1", "B2", "B3", "B4", "B5" };
	//const char* idBattery[_numberOfBattery] = { "B0", "B1", "B2" };
	//const char* idBattery[_numberOfBattery] = { "B0", "B1", "B2","B3"};
	/*char deltaVoltage_to_string[4] = {};*/
	response[4] = '\0';
	/*dtostrf(deltaVoltage[_demultiplexerPosition], 4, 2, deltaVoltage_to_string);*/
	csvTextLayOut[0] = _idMessage[0];
	strcat(csvTextLayOut, ";");
	strcat(csvTextLayOut, idBattery[_demultiplexerPosition]);
	strcat(csvTextLayOut, ";");
	strcat(csvTextLayOut, response);
	strcat(csvTextLayOut, ";;");
#if _DEBUG_FOR_SERIAL
	Serial.println(csvTextLayOut);
#endif // _DEBUG_FOR_SERIAL
}
void write_power_measurements_to_sd_card() {
	File myFile;
	if (_is_card_writing_disable)return;
	myFile = SD.open(fileName, FILE_WRITE);
	if (myFile) {
		// Keep power measurements separate from the battery voltage column.
		myFile.print(F(";;;"));
		myFile.print(stored_watts, 2);
		myFile.print(F(";"));
		myFile.println(stored_ampere, 2);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("write.WA.SD"));
#endif // _DEBUG_FOR_SERIAL
		myFile.close();
	}
	else {
#if _DEBUG_FOR_SERIAL
		Serial.println(F("err.SD"));
#endif // _DEBUG_FOR_SERIAL
		play_audio_message(AUDIO_PROBLEMA_SCHEDA_MEMORIA);
		myFile.close();
		while (true) {};
	}
}
void append_line_to_sd_card(char* message) {
	File myFile;
	if (_is_card_writing_disable)return;
	// Create/Open file
	// String fileName = "batt" + String(fileNumber) + ".csv";
	// Serial.print(F("Apro file:"));
	// Serial.println(fileName);
	myFile = SD.open(fileName, FILE_WRITE);
	if (myFile) {
		myFile.println(message);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("write.SD"));
#endif // _DEBUG_FOR_SERIAL
		myFile.close();
	}
	else {
		//buzzer_sensor_activity(5, 400, 1000, 500);
#if _DEBUG_FOR_SERIAL
		Serial.println(F("err.SD"));
#endif // _DEBUG_FOR_SERIAL
		play_audio_message(AUDIO_PROBLEMA_SCHEDA_MEMORIA);
		myFile.close();
		while (true) {};
	}
	//// Reading the file
	// myFile = SD.open("batteryValues.csv");
	// if (myFile) {
	//	//Serial.println("Read:");
	//	// Reading the whole file
	//	while (myFile.available()) {
	//		Serial.write(myFile.read());
	//	}
	//	myFile.close();
	// }
	// else {
	//	Serial.println("error opening file batteryValues.csv");
	// }
}
void trigger_all_attiny85_measurements() {
	digitalWrite(_pin_interrupt_to_attiny85, HIGH);
	delay(200);
	digitalWrite(_pin_interrupt_to_attiny85, LOW);
#if _DEBUG_FOR_SERIAL
	Serial.println(F("-Int-"));
#endif // _DEBUG_FOR_SERIAL
}
void update_max_battery_voltage(float value) {
	ii = 0;
	while (ii < _numberOfBattery) {
		// Serial.println(ii);
		if (value >= storedBatteryValues[ii]) {
#if _DEBUG_FOR_SERIAL
			/*Serial.print(F("if ")); Serial.print(value); Serial.print(F(" maggiore o uguale a ")); Serial.println(storedBatteryValues[ii]);
			Serial.print(F("metto ")); Serial.print(value); Serial.println(F(" in batteryMaxLevel"));
			 delay(1000);*/
#endif // _DEBUG_FOR_SERIAL
			batteryMaxLevel = value;
			ii++;
		}
		else {
#if _DEBUG_FOR_SERIAL
			// Serial.print(F("mando ")); Serial.print(storedBatteryValues[ii]); Serial.println(F(" in funzione"));
#endif // _DEBUG_FOR_SERIAL
			update_max_battery_voltage(storedBatteryValues[ii]);
		}
	}
}
void update_min_battery_voltage(float value) {
	ii = 0;
	while (ii < _numberOfBattery) {
		if (value <= storedBatteryValues[ii]) {
#if _DEBUG_FOR_SERIAL
			/*Serial.print(F("if ")); Serial.print(value); Serial.print(F(" maggiore o uguale a ")); Serial.println(storedBatteryValues[ii]);
			Serial.print(F("metto ")); Serial.print(value); Serial.println(F(" in batteryMaxLevel"));
			delay(1000);*/
#endif // _DEBUG_FOR_SERIAL
			batteryMinLevel = value;
			ii++;
		}
		else {
#if _DEBUG_FOR_SERIAL
			// Serial.print(F("mando ")); Serial.print(storedBatteryValues[ii]); Serial.println(F(" in funzione"));
#endif // _DEBUG_FOR_SERIAL
			update_min_battery_voltage(storedBatteryValues[ii]);
		}
	}
}
void play_audio_message(uint8_t messageCode) {
	if (_is_DPlayer_disable) return;
	DFRobotDFPlayerMini myDFPlayer;
	SoftwareSerial mySoftwareSerial(_pin_dfMiniPlayer_rx, _pin_dfMiniPlayer_tx); // rx, tx
	delay(500);
	mySoftwareSerial.begin(9600);
	delay(500);
	if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
#if _DEBUG_FOR_SERIAL
		Serial.println(F("Unable to begin:"));
		Serial.println(F("1.Please recheck the connection!"));
		Serial.println(F("2.Please insert the SD card!"));
#endif // _DEBUG_FOR_SERIAL
		//buzzer_sensor_activity(5, 2500, 80, 200);
		while (true);
	}
#if _DEBUG_FOR_SERIAL
	Serial.println(F("DFPlayer Mini online."));
#endif // _DEBUG_FOR_SERIAL
	uint16_t volume = (30.00 / 1024.00) * analogRead(_pin_dfMiniPlayer_volume);
	//Serial.println(volume);
	myDFPlayer.volume(volume);  //Set volume value. From 0 to 30
	myDFPlayer.play(messageCode);  //Play next mp3 every 3 second.
	delay(5000);
}
bool are_predicted_battery_values_acceptable() {
#if _IS_ON_AI_TEST
	x[0] = 39.36f;
	x[1] = 86.27f;
	storedBatteryValues[0] = 1.91f;
	storedBatteryValues[1] = 1.81f;
	storedBatteryValues[2] = 1.94f;
	storedBatteryValues[3] = 1.75f;
	storedBatteryValues[4] = 1.85f;
	storedBatteryValues[5] = 1.81f;
#else
	x[0] = stored_ampere;
	x[1] = stored_watts;
#endif // _IS_ON_AI_TEST
	x[0] = log(x[0] + 1.0f) / 10.0f;
	x[1] = log(x[1] + 1.0f) / 10.0f;
	run_neural_network_forward_pass();
	for (int i = 0; i < 6; i++) {
		y[i] = y[i] * 10.00f;
#if _DEBUG_FOR_SERIAL
		Serial.println(y[i]);
		Serial.println(storedBatteryValues[i]);
#endif // _DEBUG_FOR_SERIAL
	}
	float mse = mean_squared_error(storedBatteryValues, y, numberOf_Y);
	float observed_mean = calculate_arithmetic_mean(storedBatteryValues, numberOf_Y);
#if _DEBUG_FOR_SERIAL
	Serial.print(F("mse: ")); Serial.println(mse);
	Serial.print(F("observed_mean: ")); Serial.println(observed_mean);
#endif // _DEBUG_FOR_SERIAL
	uint16_t percentage = calculate_normalized_rmse_percentage(mse, observed_mean);
#if _DEBUG_FOR_SERIAL
	Serial.print(F("% :")); Serial.println(percentage);
#endif // _DEBUG_FOR_SERIAL
	if (percentage < max_AI_error_percentage) {
		return true;
	}
	else { return false; }
}
#if _DEBUG_FOR_SERIAL
int get_free_sram_bytes() {
	extern int __heap_start, * __brkval;
	int v;
	return (int)&v - (__brkval ? (int)__brkval : (int)&__heap_start);
}
#endif // _DEBUG_FOR_SERIAL
