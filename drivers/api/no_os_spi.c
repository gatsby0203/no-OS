/***************************************************************************//**
 *   @file   no_os_spi.c
 *   @brief  Implementation of the SPI Interface
 *   @author Antoniu Miclaus (antoniu.miclaus@analog.com)
********************************************************************************
 * Copyright 2020(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include <inttypes.h>
#include "no_os_spi.h"
#include <stdlib.h>
#include "no_os_error.h"
#include "no_os_mutex.h"
#include "no_os_alloc.h"

/**
 * @brief spi_table contains the pointers towards the SPI buses
*/
static void *spi_table[SPI_MAX_BUS_NUMBER + 1];

/**
 * @brief Initialize the SPI communication peripheral.
 * @param desc - The SPI descriptor.
 * @param param - The structure that contains the SPI parameters.
 * @return 0 in case of success, -1 otherwise.
 */
int32_t no_os_spi_init(struct no_os_spi_desc **desc,
		       const struct no_os_spi_init_param *param)
{
	int32_t ret;

	if (!param || !param->platform_ops)
		return -EINVAL;

	if (!param->platform_ops->init)
		return -ENOSYS;
	if (param->device_id > SPI_MAX_BUS_NUMBER)
		return -EINVAL;
	// Initializing BUS descriptor
	if (!spi_table[param->device_id]) {
		ret = no_os_spibus_init(param);
		if (ret)
			return ret;
	}
	// Initilize SPI descriptor
	ret = param->platform_ops->init(desc, param);
	if (ret)
		return ret;
	(*desc)->bus = spi_table[param->device_id];
	(*desc)->platform_ops = param->platform_ops;
	(*desc)->parent = param->parent;

	return 0;
}

/**
 * @brief Initialize the SPI bus communication peripheral.
 * @param param - The structure that containes the SPI bus parameters
 * @return 0 in case of success, error code otherwise
*/
int32_t no_os_spibus_init(const struct no_os_spi_init_param *param)
{
	struct no_os_spibus_desc *bus = (struct no_os_spibus_desc *)no_os_calloc(1,
					sizeof(struct no_os_spibus_desc));

	if (!bus)
		return -ENOMEM;

	no_os_mutex_init(bus->mutex);

	bus->device_id = param->device_id;
	bus->max_speed_hz = param->max_speed_hz;
	bus->mode = param->mode;
	bus->bit_order = param->bit_order;
	bus->platform_ops = param->platform_ops;
	bus->extra = param->extra;

	spi_table[param->device_id] = bus;

	return 0;
}

/**
 * @brief Free the resources allocated by no_os_spi_init().
 * @param desc - The SPI descriptor.
 * @return 0 in case of success, -1 otherwise.
 */
int32_t no_os_spi_remove(struct no_os_spi_desc *desc)
{
	// Remove SPI bus
	no_os_spibus_remove(desc->device_id);

	if (!desc || !desc->platform_ops)
		return -EINVAL;

	if (!desc->platform_ops->remove)
		return -ENOSYS;
	return desc->platform_ops->remove(desc);
}

/**
 * @brief Removes SPI bus instance
 * @param bus_number - SPI bus number
*/
void no_os_spibus_remove(uint32_t bus_number)
{
	struct no_os_spibus_desc *spi_bus_desriptor = (struct no_os_spibus_desc *)
			spi_table[bus_number];

	no_os_mutex_remove(spi_bus_desriptor->mutex);

	if (spi_bus_desriptor) {
		no_os_free(spi_bus_desriptor);
		spi_bus_desriptor = 0;
	}
}

/**
 * @brief Write and read data to/from SPI.
 * @param desc - The SPI descriptor.
 * @param data - The buffer with the transmitted/received data.
 * @param bytes_number - Number of bytes to write/read.
 * @return 0 in case of success, -1 otherwise.
 */
int32_t no_os_spi_write_and_read(struct no_os_spi_desc *desc,
				 uint8_t *data,
				 uint16_t bytes_number)
{
	int32_t ret;

	if (!desc || !desc->platform_ops)
		return -EINVAL;

	if (!desc->platform_ops->write_and_read)
		return -ENOSYS;

	no_os_mutex_lock(desc->bus->mutex);
	ret =  desc->platform_ops->write_and_read(desc, data, bytes_number);
	no_os_mutex_unlock(desc->bus->mutex);

	return ret;
}

/**
 * @brief  Iterate over head list and send all spi messages
 * @param desc - The SPI descriptor.
 * @param msgs - Array of messages.
 * @param len - Number of messages in the array.
 * @return 0 in case of success, negativ error code otherwise.
 */
int32_t no_os_spi_transfer(struct no_os_spi_desc *desc,
			   struct no_os_spi_msg *msgs,
			   uint32_t len)
{
	int32_t  ret;
	uint32_t i;

	if (!desc || !desc->platform_ops)
		return -EINVAL;

	if (desc->platform_ops->transfer)
		return desc->platform_ops->transfer(desc, msgs, len);

	no_os_mutex_lock(desc->bus->mutex);

	for (i = 0; i < len; i++) {
		if (msgs[i].rx_buff != msgs[i].tx_buff || !msgs[i].tx_buff) {
			ret = -EINVAL;
			goto out;
		}
		ret = no_os_spi_write_and_read(desc, msgs[i].rx_buff,
					       msgs[i].bytes_number);
		if (NO_OS_IS_ERR_VALUE(ret)) {
			goto out;
		}
	}

out:
	no_os_mutex_unlock(desc->bus->mutex);
	return 0;
}
