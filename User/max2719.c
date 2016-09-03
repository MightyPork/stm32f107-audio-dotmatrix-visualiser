#include <stdbool.h>
#include <spi.h>
#include "max2719.h"

// TODO convert to buffer + batch write.
// TODO store hspi in the max2719 struct, so it can be used.

static inline
void send_byte(MAX2719_Cfg *inst, uint8_t b)
{
	//inst->SPIx->DR = b;
	//while (!(inst->SPIx->SR & SPI_SR_TXE));

	// FIXME figure out why regular transmit is not working
	HAL_SPI_Transmit(&hspi1, &b, 1, 10);
}


static inline
void set_nss(MAX2719_Cfg *inst, bool nss)
{
	if (nss) {
		inst->CS_GPIOx->BSRR = inst->CS_PINx;
	} else {
		inst->CS_GPIOx->BRR = inst->CS_PINx;
	}
}


static void send_word(MAX2719_Cfg *inst, MAX2719_Command cmd, uint8_t data)
{
	send_byte(inst, cmd);
	send_byte(inst, data);
}


void max2719_cmd(MAX2719_Cfg *inst, uint32_t nth, MAX2719_Command cmd, uint8_t data)
{
	set_nss(inst, 0);
	while (inst->SPIx->SR & SPI_SR_BSY);

	for (uint32_t i = 0; i < inst->chain_len; i++) {
		if (i == inst->chain_len - nth - 1) {
			send_word(inst, cmd, data);
		} else {
			send_word(inst, MAX2719_CMD_NOOP, 0);
		}
	}

	while (inst->SPIx->SR & SPI_SR_BSY);
	set_nss(inst, 1);
}



void max2719_cmd_all(MAX2719_Cfg *inst, MAX2719_Command cmd, uint8_t data)
{
	set_nss(inst, 0);
	while (inst->SPIx->SR & SPI_SR_BSY);

	for (uint32_t i = 0; i < inst->chain_len; i++) {
		send_word(inst, cmd, data);
	}

	while (inst->SPIx->SR & SPI_SR_BSY);
	set_nss(inst, 1);
}


void max2719_cmd_all_data(MAX2719_Cfg *inst, MAX2719_Command cmd, uint8_t *data)
{
	set_nss(inst, 0);
	while (inst->SPIx->SR & SPI_SR_BSY);

	for (uint32_t i = 0; i < inst->chain_len; i++) {
		send_word(inst, cmd, data[inst->chain_len - i - 1]);
	}

	while (inst->SPIx->SR & SPI_SR_BSY);
	set_nss(inst, 1);
}
