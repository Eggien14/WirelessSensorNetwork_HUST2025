#include "sx1278_lora.h"

/* ===================================================================================================
 * @brief: READ Register by address, store data in output pointer
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	pAddr: pointer to Register address
 * @param:	r_length: length of register address
 * @param:	pOutput: pointer to output data buffer
 * @param:	w_length: length of receive data
 *
 * @return: void
 ======================================================================================================*/
void LoRa_readReg(LoRa* _LoRa, uint8_t* pAddr, uint16_t r_length, uint8_t* pOutput, uint16_t w_length){

	//1) Pull CS_pin of LoRa module to LOW to indicate the begin of SPI communication
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 0);

	//2) Set Register address to read
	HAL_SPI_Transmit(_LoRa->hSPI, pAddr, r_length, TRANSMIT_TIMEOUT);

	//3) Wait finish
	while(HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);

	//4) Receive data -> store to array *pOutput
	HAL_SPI_Receive(_LoRa->hSPI, pOutput, w_length, RECEIVE_TIMEOUT);

	//5) Wait finish
	while(HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);

	//6) Pull CS_pin of LoRa module to HIGH to indicate the end of SPI communication
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 1);
}



/* ===================================================================================================
 * @brief:	WRITE data to a Register
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	pAddr: pointer to Register address
 * @param:	r_length: length of register address
 * @param:	pData: pointer to sending data buffer
 * @param:	w_length: length of sending data
 *
 * @return: void
 ======================================================================================================*/
void LoRa_writeReg(LoRa* _LoRa, uint8_t* pAddr, uint16_t r_length, uint8_t* pData, uint16_t w_length){

	//1) Pull CS_pin of LoRa module to LOW to indicate the begin of SPI communication
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 0);

	//2) Set Register address to read
	HAL_SPI_Transmit(_LoRa->hSPI, pAddr, r_length, TRANSMIT_TIMEOUT);

	//3) Wait finish
	while(HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);

	//4) Write data
	HAL_SPI_Transmit(_LoRa->hSPI, pData, w_length, TRANSMIT_TIMEOUT);

	//5) Wait finish
	while(HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);

	//6) Pull CS_pin of LoRa module to HIGH to indicate the end of SPI communication
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 1);
}


/* ===================================================================================================
 * @brief: READ a register (1 byte) by address
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	address: address of the register in hex
 *
 * @return:	register value
 ======================================================================================================*/
uint8_t LoRa_read(LoRa* _LoRa, uint8_t address){

	uint8_t	addr;
	uint8_t read_data;

	//MSB of address is to indicate read or write mode: "0" - READ; "1" - WRITE
	addr = address & 0x7F;
	LoRa_readReg(_LoRa, &addr, 1, &read_data, 1);

	return read_data;
}


/* ===================================================================================================
 * @brief: WRITE a register (1 byte) by address
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	address: address of the register in hex
 * @param:	value: data value you want to write
 *
 * @return:	void
 ======================================================================================================*/
void LoRa_write(LoRa* _LoRa, uint8_t address, uint8_t value){

	uint8_t	addr;
	uint8_t data;

	//MSB of address is to indicate read or write mode: "0" - READ; "1" - WRITE
	addr = address | 0x80;
	data = value;
	LoRa_writeReg(_LoRa, &addr, 1, &data, 1);
}


/* ===================================================================================================
 * @brief: WRITE a register by address (burst write)
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	address: address of the register in hex
 * @param:	pValue: pointer to data value you want to write
 * @param:	length: number of bytes in data value
 *
 * @return:	void
 ======================================================================================================*/
void LoRa_BurstWrite(LoRa* _LoRa, uint8_t address, uint8_t *pValue, uint8_t length){
	uint8_t addr;
	addr = address | 0x80;

	//NSS = 1
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 0);

	HAL_SPI_Transmit(_LoRa->hSPI, &addr, 1, TRANSMIT_TIMEOUT);
	while (HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);
	//Write data in FiFo
	HAL_SPI_Transmit(_LoRa->hSPI, pValue, length, TRANSMIT_TIMEOUT);
	while (HAL_SPI_GetState(_LoRa->hSPI) != HAL_SPI_STATE_READY);
	//NSS = 0
	HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, 1);
}


/* ===================================================================================================
 * @brief:	Set LoRa Operation mode
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	mode: select operation mode
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setMode(LoRa* _LoRa, int mode){
	uint8_t    read;
	uint8_t    data;

	read = LoRa_read(_LoRa, RegOpMode);
	data = read;

	if(mode == SLEEP_MODE){
		data = (read & 0xF8) | 0x00;
		_LoRa->current_mode = SLEEP_MODE;
	}else if (mode == STNBY_MODE){
		data = (read & 0xF8) | 0x01;
		_LoRa->current_mode = STNBY_MODE;
	}else if (mode == TRANSMIT_MODE){
		data = (read & 0xF8) | 0x03;
		_LoRa->current_mode = TRANSMIT_MODE;
	}else if (mode == RXCONTIN_MODE){
		data = (read & 0xF8) | 0x05;
		_LoRa->current_mode = RXCONTIN_MODE;
	}else if (mode == RXSINGLE_MODE){
		data = (read & 0xF8) | 0x06;
		_LoRa->current_mode = RXSINGLE_MODE;
	}
	// Change RegOpMode register value
	LoRa_write(_LoRa, RegOpMode, data);
};



/* ===================================================================================================
 * @brief:	Reset LoRa module
 *
 * @param:	_LoRa: pointer to LoRa data struct
 *
 * @return: none
 ======================================================================================================*/
void LoRa_reset(LoRa* _LoRa){
	HAL_GPIO_WritePin(_LoRa->reset_port, _LoRa->reset_pin, 0);
	HAL_Delay(1);
	HAL_GPIO_WritePin(_LoRa->reset_port, _LoRa->reset_pin, 1);
	HAL_Delay(100);
}


/* ===================================================================================================
 * @brief:	Set carrier frequency
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	frequency (Mhz)
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setFrequency(LoRa* _LoRa, int freq){
	uint8_t  data;
	uint32_t F;
	//Convert to 24-bit value (datashet)
	F = (freq * 524288)>>5;

	// write Msb:
	data = F >> 16;
	LoRa_write(_LoRa, RegFrMsb, data);
	HAL_Delay(3);

	// write Mid:
	data = F >> 8;
	LoRa_write(_LoRa, RegFrMid, data);
	HAL_Delay(3);

	// write Lsb:
	data = F >> 0;
	LoRa_write(_LoRa, RegFrLsb, data);
	HAL_Delay(3);
}


/* ===================================================================================================
 * @brief:	Set the LowDataRateOptimization flag, HIGH whenever symbol duration (Tsymbol) execeeds 16ms
 * 																					- datasheet 31, 28
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	value: 0 to disable, 1 to enable
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setLowDaraRateOptimization(LoRa* _LoRa, uint8_t value){
	uint8_t	data;
	uint8_t	read;

	//1) Get the current value of RegModemConfig3 register
	read = LoRa_read(_LoRa, RegModemConfig3);

	//2) Enable/Disable Low Data Rate Optimize (LDO) mode
	if(value){
		data = read | 0x08;	//0000 0100 -> enable 3th bit
	} else {
		data = read & 0xF7;	//1111 1011 -> disable 3th bit
	}

	//4) Change RegModemConfig3 value
	LoRa_write(_LoRa, RegModemConfig3, data);
	HAL_Delay(10);
}


/* ===================================================================================================
 * @brief:	Set the LowDataRateOptimization flag
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	value: 0 to disable, 1 to enable
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setAutoLDO(LoRa* _LoRa){
	//Possible bandwidth
	double BW[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0};

	// T_symbol = 2^SF / BW (datasheet 28)
	long T_symbol = (long)((1 << _LoRa->spredingFactor) / ((double)BW[_LoRa->bandWidth]));

	// Compare with 16 ms, if larger -> enable Low Data Rate Optimize (LDO) mode
	LoRa_setLowDaraRateOptimization(_LoRa, T_symbol > 16.0);
}


/* ===================================================================================================
 * @brief:	Set the spreading factor, limited from 7 to 12
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	SF: Spreading factor
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setSpreadingFactor(LoRa* _LoRa, int SF){
	uint8_t	data;
	uint8_t	read;

	//Limited from 7 to 12
	if (SF > 12){
		SF = 12;
	}else if (SF < 7){
		SF = 7;
	}

	//Get the current state
	read = LoRa_read(_LoRa, RegModemConfig2);
	HAL_Delay(5);

	data = (SF << 4) + (read & 0x0F);
	LoRa_write(_LoRa, RegModemConfig2, data);
	HAL_Delay(5);

	LoRa_setAutoLDO(_LoRa);
}


/* ===================================================================================================
 * @brief:	Set power gain
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	power: Power gain (ex: 0xFC)
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setPower(LoRa* _LoRa, uint8_t power){
	LoRa_write(_LoRa, RegPaConfig, power);
	HAL_Delay(5);
}


/* ===================================================================================================
 * @brief:	Set maximum allowed current
 *
 * @param:	_LoRa: pointer to LoRa data struct
 * @param:	current: desired max currnet in mA
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setOCP(LoRa* _LoRa, uint8_t current){
	uint8_t	OcpTrim = 0;

	if(current<45)
		current = 45;
	if(current>240)
		current = 240;

	if(current <= 120)
		OcpTrim = (current - 45)/5;
	else if(current <= 240)
		OcpTrim = (current + 30)/10;

	OcpTrim = OcpTrim + (1 << 5);
	LoRa_write(_LoRa, RegOcp, OcpTrim);
	HAL_Delay(10);
}


/* ===================================================================================================
 * @brief:	Set timeout msb to 0xFF + set CRC enable
 *
 * @param:	_LoRa: pointer to LoRa data struct
 *
 * @return: none
 ======================================================================================================*/
void LoRa_setTOMsb_setCRCon(LoRa* _LoRa){
	uint8_t read, data;

	read = LoRa_read(_LoRa, RegModemConfig2);

	data = read | 0x07;
	LoRa_write(_LoRa, RegModemConfig2, data);
	HAL_Delay(10);
}


/* ===================================================================================================
 * @brief:	Initialize and config LoRa module with param in _LoRa struct
 *
 * @param:	_LoRa: pointer to LoRa data struct
 *
 * @return: After call this function, LoRa module is in STANDBY mode, ready to TX and RX right the way
 ======================================================================================================*/
uint16_t LoRa_init(LoRa* _LoRa){
	uint8_t    data;
	uint8_t    read;

	if(_LoRa){
		// goto sleep mode:
			LoRa_setMode(_LoRa, SLEEP_MODE);
			HAL_Delay(10);

		// turn on LoRa mode:
			read = LoRa_read(_LoRa, RegOpMode);
			HAL_Delay(10);
			data = read | 0x80;
			LoRa_write(_LoRa, RegOpMode, data);
			HAL_Delay(100);

		// set frequency:
			LoRa_setFrequency(_LoRa, _LoRa->frequency);

		// set output power gain:
			LoRa_setPower(_LoRa, _LoRa->power);

		// set over current protection:
			LoRa_setOCP(_LoRa, _LoRa->overCurrentProtection);

		// set LNA gain:
			LoRa_write(_LoRa, RegLna, 0x23);

		// set spreading factor, CRC on, and Timeout Msb:
			LoRa_setTOMsb_setCRCon(_LoRa);
			LoRa_setSpreadingFactor(_LoRa, _LoRa->spredingFactor);

		// set Timeout Lsb:
			LoRa_write(_LoRa, RegSymbTimeoutLsb, 0xFF);

		// set bandwidth, coding rate and expilicit mode:
			// 8 bit RegModemConfig --> | X | X | X | X | X | X | X | X |
			//       bits represent --> |   bandwidth   |     CR    |I/E|
			data = 0;
			data = (_LoRa->bandWidth << 4) + (_LoRa->crcRate << 1);
			LoRa_write(_LoRa, RegModemConfig1, data);
			LoRa_setAutoLDO(_LoRa);

		// set preamble:
			LoRa_write(_LoRa, RegPreambleMsb, _LoRa->preamble >> 8);
			LoRa_write(_LoRa, RegPreambleLsb, _LoRa->preamble >> 0);

		// DIO mapping:   --> DIO: RxDone
			read = LoRa_read(_LoRa, RegDioMapping1);
//			data = read | 0x3F;		//00111111
			data = (read & 0xFC) | 0x00;
			LoRa_write(_LoRa, RegDioMapping1, data);

		// goto standby mode:
			LoRa_setMode(_LoRa, STNBY_MODE);
			_LoRa->current_mode = STNBY_MODE;
			HAL_Delay(10);

			read = LoRa_read(_LoRa, RegVersion);
			if(read == 0x12)
				return LORA_OK;
			else
				return LORA_NOT_FOUND;
	}
	else {
		return LORA_UNAVAILABLE;
	}
}


/* ===================================================================================================
 * @brief:	Return the latest RSSI value of last received packet
 *
 * @param:	_LoRa: pointer to LoRa data struct
 *
 * @return:	Return the latest RSSI value of last received packet
 ======================================================================================================*/
int LoRa_getRSSI(LoRa* _LoRa){
	uint8_t read;
	read = LoRa_read(_LoRa, RegPktRssiValue);
	return -164 + read;
}


/* ===================================================================================================
 * @brief:	Transmit data packet
 *
 * @param:	_LoRa: pointer to LoRa data struct
 *
 * @return:	Return the latest RSSI value of last received packet
 ======================================================================================================*/
uint8_t LoRa_transmit(LoRa* _LoRa, uint8_t* pData, uint8_t length, uint16_t timeout){
	uint8_t read;

	//Save the current mode to return after TX done
	int mode = _LoRa->current_mode;

	//OP Mode->STANDBY; FiFo data buffer can only be changed in STANBY mode
	LoRa_setMode(_LoRa, STNBY_MODE);

	//Read value of RegFiFoTxBaseAddr register (which store the value of the start addr of TX FIFO memory)
	read = LoRa_read(_LoRa, RegFiFoTxBaseAddr);

	// Config the FIFO pointer, pointing at the start TX FIFO memory
	LoRa_write(_LoRa, RegFiFoAddPtr, read);

	//Config the length of the payload
	LoRa_write(_LoRa, RegPayloadLength, length);

	//Load data into the FIFO register to prepare for transmission
	LoRa_BurstWrite(_LoRa, RegFiFo, pData, length);

	//Change mode: STANDBY -> TRANSMIT (Note that register value can only be changed in STANDBY mode)
	LoRa_setMode(_LoRa, TRANSMIT_MODE);

	// Wait for TxDone flag -> indicate that TX is fuking done
	while(1){
		read = LoRa_read(_LoRa, RegIrqFlags);
		//0x08 = 0000 1000 -> Check if the Bit 3 of RegIrqFlags (TxDone - datasheet) is HIGH or not
		if((read & 0x08)!=0){
			//Delete flag
			LoRa_write(_LoRa, RegIrqFlags, 0xFF);
			//Back to the previous mode
			LoRa_setMode(_LoRa, mode);
			return 1;
		}
		else{
			//**IMPORTANT**
			//if TX fail (TxDone never HIGH) -> auto get out of TRANSMIT after timeout, handle shit after this
			if(--timeout==0){
				LoRa_setMode(_LoRa, mode);
				return 0;
			}
		}
		HAL_Delay(1);
	}
}


/* ===================================================================================================
 * @brief:	Receive data packet
 *
 * 		arguments   :
			LoRa*    LoRa     --> LoRa object handler
			uint8_t  data			--> A pointer to the array that you want to write bytes in it
			uint8_t	 length   --> Determines how many bytes you want to read
 *
 * @return:	The number of bytes received
 ======================================================================================================*/
uint8_t LoRa_receive(LoRa* _LoRa, uint8_t* data, uint8_t length){
	uint8_t read;
	uint8_t number_of_bytes;
	uint8_t min = 0;

	//Create buffer
	for(int i=0; i<length; i++){
		data[i]=0;
	}
	//Change OP mode to STANDBY
	LoRa_setMode(_LoRa, STNBY_MODE);

	// Read RegIrqFlags and look for RxDone flag on bit 6
	read = LoRa_read(_LoRa, RegIrqFlags);

	//0x40 = 0100 0000 -> check whether RxDone is HIGH
	if((read & 0x40) != 0){
		//Delete flag
		LoRa_write(_LoRa, RegIrqFlags, 0xFF);
		//Get the length of the payload (store in the RegRxNbBytes register)
		number_of_bytes = LoRa_read(_LoRa, RegRxNbBytes);
		//Get the start addr of the memory that store data recieve
		read = LoRa_read(_LoRa, RegFiFoRxCurrentAddr);
		//Load memory address to FIFO pointer
		LoRa_write(_LoRa, RegFiFoAddPtr, read);

		min = length >= number_of_bytes ? number_of_bytes : length;
		for(int i=0; i<min; i++)
			data[i] = LoRa_read(_LoRa, RegFiFo);
	}
	LoRa_setMode(_LoRa, RXCONTIN_MODE);
    return min;
}
