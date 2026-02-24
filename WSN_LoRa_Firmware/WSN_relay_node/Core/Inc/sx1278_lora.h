/*
 **All details to #define and stuff can be found within the sx1278 datasheet**
https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001Rbr/6EfVZUorrpoKFfvaF_Fkpgp5kzjiNyiAbqcpqh9qSjE
*/

#ifndef INC_SX1278_LORA_H_
#define INC_SX1278_LORA_H_

#include "main.h"

#define TRANSMIT_TIMEOUT		2000
#define RECEIVE_TIMEOUT			2000


//----------- MODES -------------//
#define SLEEP_MODE				0
#define	STNBY_MODE				1
#define TRANSMIT_MODE			3
#define RXCONTIN_MODE			5
#define RXSINGLE_MODE			6


//-------- BANDWIDTH ----------//
#define BW_7_8KHz				0
#define BW_10_4KHz				1
#define BW_15_6KHz				2
#define BW_20_8KHz				3
#define BW_31_25KHz				4
#define BW_41_7KHz				5
#define BW_62_5KHz				6
#define BW_125KHz				7
#define BW_250KHz				8
#define BW_500KHz				9


//-------- CODING RATE ----------//
#define CR_4_5					1
#define CR_4_6					2
#define CR_4_7					3
#define CR_4_8					4


//------ SPREADING FACTORS -------//
#define SF_7					7
#define SF_8					8
#define SF_9					9
#define SF_10					10
#define SF_11  					11
#define SF_12					12


//--------- POWER GAIN ----------//
#define POWER_11db				0xF6
#define POWER_14db				0xF9
#define POWER_17db				0xFC
#define POWER_20db				0xFF


//--------- REGISTERS ---------//

//Registers for RF block
#define RegFiFo					0x00		//First-in first-out data input/output
#define RegOpMode				0x01		//LoRa's Operation mode selection
#define RegFrMsb				0x06		//MSB of RF carrier frequency
#define RegFrMid				0x07		//Intermediate bit RF carrier frequency
#define RegFrLsb				0x08		//LSB of RF carrier frequency
#define RegPaConfig				0x09
#define RegOcp					0x0B
#define RegLna					0x0C

//Registers for SPI control
#define RegFiFoAddPtr			0x0D		//SPI address pointer in FIFO data buffer
#define RegFiFoTxBaseAddr		0x0E		//Write base addr for TX modulator
#define RegFiFoRxBaseAddr		0x0F		//Read base addr for RX demodulator
#define RegFiFoRxCurrentAddr	0x10		//Start address (in data buffer) of last packet received

#define RegIrqFlags				0x12		//Interrupt flags
#define RegRxNbBytes			0x13		//Number of payload bytes of latest packet received
#define RegPktRssiValue			0x1A		//RSSI of the latest packet received (dBm)
#define RegRssiValue			0x1A		//Current RSSI value (dBm)

#define	RegModemConfig1			0x1D		//Config Signal bandwidth - 0x07 or CodingRate - '001'
#define RegModemConfig2			0x1E		//Config SpreadingFactor(SF) - 0x07, enable CRC - 0x00, TxContinuousMode: 0-> normal; 1 -> continuous mode (send multiple packet)
#define RegSymbTimeoutLsb		0x1F		//RX Time-Out LSB
#define RegPreambleMsb			0x20		//Preamble length MSB
#define RegPreambleLsb			0x21		//Preamble length LSB
#define RegPayloadLength		0x22		//Payload length in bytes
#define RegModemConfig3			0x26		//Config Low Data Rate Optimize (LDO) mode

#define RegFeiMsb				0x28		//Estimated frequency error MSB
#define RegFeiLsb				0x2A		//Estimated frequency error LSB
#define RegSyncWord				0x39
#define RegDioMapping1			0x40
#define RegDioMapping2			0x41
#define RegVersion				0x42


//---------- LORA STATUS ---------//
#define LORA_OK					200
#define LORA_NOT_FOUND			404
#define LORA_LARGE_PAYLOAD		413
#define LORA_UNAVAILABLE		503


//-----------LORA CONFIG STRUCT ---------//
typedef struct LoRa_setting{
	//Hardware setting
	GPIO_TypeDef* 		CS_port;
	uint16_t			CS_pin;
	GPIO_TypeDef*		reset_port;
	uint16_t			reset_pin;
	GPIO_TypeDef*		DIO0_port;
	uint16_t			DIO0_pin;
	SPI_HandleTypeDef*	hSPI;

	//Firmware setting
	int				current_mode;
	int				frequency;
	uint8_t			spredingFactor;
	uint8_t			bandWidth;
	uint8_t			crcRate;
	uint16_t		preamble;
	uint8_t			power;
	uint8_t			overCurrentProtection;

} LoRa;

void LoRa_readReg(LoRa* _LoRa, uint8_t* pAddr, uint16_t r_length, uint8_t* pOutput, uint16_t w_length);
void LoRa_writeReg(LoRa* _LoRa, uint8_t* pAddr, uint16_t r_length, uint8_t* pData, uint16_t w_length);
uint8_t LoRa_read(LoRa* _LoRa, uint8_t address);
void LoRa_write(LoRa* _LoRa, uint8_t address, uint8_t value);
void LoRa_BurstWrite(LoRa* _LoRa, uint8_t address, uint8_t *pValue, uint8_t length);
void LoRa_setMode(LoRa* _LoRa, int mode);
void LoRa_reset(LoRa* _LoRa);
void LoRa_setFrequency(LoRa* _LoRa, int freq);
void LoRa_setLowDaraRateOptimization(LoRa* _LoRa, uint8_t value);
void LoRa_setAutoLDO(LoRa* _LoRa);
void LoRa_setSpreadingFactor(LoRa* _LoRa, int SF);
void LoRa_setPower(LoRa* _LoRa, uint8_t power);
void LoRa_setOCP(LoRa* _LoRa, uint8_t current);
void LoRa_setTOMsb_setCRCon(LoRa* _LoRa);
uint16_t LoRa_init(LoRa* _LoRa);
int LoRa_getRSSI(LoRa* _LoRa);
uint8_t LoRa_transmit(LoRa* _LoRa, uint8_t* pData, uint8_t length, uint16_t timeout);
uint8_t LoRa_receive(LoRa* _LoRa, uint8_t* data, uint8_t length);


#endif /* INC_SX1278_LORA_H_ */
