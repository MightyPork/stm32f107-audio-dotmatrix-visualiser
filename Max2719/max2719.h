#ifndef MAX2719_H
#define MAX2719_H

#include "stm32f1xx_hal.h"

/** Generic utilities for controlling the MAX2719 display driver */

typedef struct {
	SPI_TypeDef *SPIx; /*!< SPI iface used by this instance */
	GPIO_TypeDef *CS_GPIOx; /*!< Chip select GPIO port */
	uint16_t CS_PINx; /*!< Chip select pin mask */
	uint32_t chain_len; /*!< Number of daisy-chained drivers (for "all" or "n-th" commands */
} MAX2719_Cfg;


typedef enum {
	MAX2719_CMD_NOOP = 0x00,

	MAX2719_CMD_DIGIT0 = 0x01,
	MAX2719_CMD_DIGIT1 = 0x02,
	MAX2719_CMD_DIGIT2 = 0x03,
	MAX2719_CMD_DIGIT3 = 0x04,
	MAX2719_CMD_DIGIT4 = 0x05,
	MAX2719_CMD_DIGIT5 = 0x06,
	MAX2719_CMD_DIGIT6 = 0x07,
	MAX2719_CMD_DIGIT7 = 0x08,

	MAX2719_CMD_DECODE_MODE = 0x09,
	MAX2719_CMD_INTENSITY = 0x0A,
	MAX2719_CMD_SCAN_LIMIT = 0x0B,
	MAX2719_CMD_SHUTDOWN = 0x0C,
	MAX2719_CMD_DISPLAY_TEST = 0x0F,
} MAX2719_Command;


/**
 * @brief Send a command to a single driver
 * @param inst : config struct
 * @param nth  : driver index
 * @param cmd  : command
 * @param data : data byte
 */
void max2719_cmd(MAX2719_Cfg *inst, uint32_t nth, MAX2719_Command cmd, uint8_t data);

/**
 * @brief Send command to all drivers, with the same data
 * @param inst : config struct
 * @param cmd  : command
 * @param data : data byte
 */
void max2719_cmd_all(MAX2719_Cfg *inst, MAX2719_Command cmd, uint8_t data);

/**
 * @brief Send command to all drivers, with varying data
 * @param inst : config struct
 * @param cmd  : command
 * @param data : array of data bytes (must be long to cover all drivers)
 */
void max2719_cmd_all_data(MAX2719_Cfg *inst, MAX2719_Command cmd, uint8_t *data);

#endif // MAX2719_H
