/*=============================================================================
    start of file
=============================================================================*/

/*******************************************************************************
 * Copyright (c) 2020 OpenSynergy GmbH.
 *
 * This software may not be used in any way or distributed without
 * permission. All rights reserved.
 ******************************************************************************/

/**
 *@file      tunsdk_ipersistency.h $RelUrl:  $
 *@version   $LastChangedRevision:  $
 *
 *@brief This file contains the declaration of CTUNSDK.
 *
 */


#ifndef TUNSDK_IPERSISTENCY_H_
#define TUNSDK_IPERSISTENCY_H_

#include "tunsdk_types.h"

/* tunsdk_ipersistency uses C linkage*/
#ifdef __cplusplus
extern "C" {
#endif

/**  @addtogroup TUNSDK_IPERSISTENCY
  *  This group groups all APIs that are part of TUNSDK_IPERSISTENCY.
  *  @{
  */

/**
 *@brief Initialize the IPERSISTENCY. IPERSISTENCY shall take measures to be able to handle future calls via the IPERSISTENCY interface.
 *@return #TUNSDK_STATUS_OK in case persistency could be started, #TUNSDK_STATUS_FAIL otherwise
 */
Te_tunsdk_status TUNSDK_IPERSISTENCY_Initialize(void);

/**
 * @brief Store data persistently
 * @param[in] file_id_app     File identifier
 * @param[in] buffer          Pointer to a buffer to be stored persistently
 * @param[in] data_size       Size of the data to be stored persistently
 * @param[out] written_bytes  number of bytes written into the persistence
 *
 * @return #TUNSDK_STATUS_OK in case of data was written successfully, #TUNSDK_STATUS_FAIL otherwise
 *
 * @par Description
 *      This function stores a buffer persistently in a defined storage. This can
 *      be a FLASH, MMC or other persistence data device. Also a RAM cached functionality
 *      is allowed where data is flashed persistently during shutdown or on flush request.
 *      The system must ensure the  availability of  the written data after restart,
 *      reset, system crash or other power off conditions. The data is identified by a
 *      data ID. This ID number can be mapped in the interface implementation to an identifier
 *      (e.g. internal ID, filename, flash drive, memory address).
 *      The implementation of the interface has to ensure the data consistency and shall
 *      detect any kind of data corruption. In case of corrupted data it has to provide a
 *      suitable error code. Before this function returns the data is written into the persistence.
 */
Te_tunsdk_status  TUNSDK_IPERSISTENCY_WriteData(uint32_t file_id_app, const uint8_t* buffer, uint32_t data_size, uint32_t& written_bytes);

/**
 * @brief Read data from the persistence
 * @param[in] file_id_app            File identifier
 * @param[in] data_size              Size of the data read
 * @param[in] receive_buffer         Pointer to a buffer where to copy the data
 * @param[in] receiver_buffer_size   Size of the receive buffer
 * @param[out] read_bytes            number of bytes read from the persistence
 *
 * @return #TUNSDK_STATUS_OK in case of up to data_size bytes have been read, #TUNSDK_STATUS_FAIL otherwise
 *
 * @par Description
 *      This function read up to receiver_buffer_size bytes from the persistence
 *      into the receive_buffer. The system shall ensure data consistency.
 *      In case of data corruption this function shall return
 *      #TUNSDK_STATUS_FAIL.
 */
Te_tunsdk_status TUNSDK_IPERSISTENCY_ReadData(uint32_t file_id_app, uint32_t data_size, uint8_t* receive_buffer, uint32_t receiver_buffer_size, uint32_t& read_bytes);

/**
 * @brief This function is used to erase data from an NVM ID
 * @param[in] file_id   File identifier
 *
 * @return #TUNSDK_STATUS_OK in case data was deleted, #TUNSDK_STATUS_FAIL otherwise
 *
 * @par Description
 * This function erase the persisted data.
 */
Te_tunsdk_status TUNSDK_IPERSISTENCY_DeleteData(uint32_t file_id);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* TUNSDK_IPERSISTENCY_H_ */

/******************************************************************************
    end of file
******************************************************************************/
