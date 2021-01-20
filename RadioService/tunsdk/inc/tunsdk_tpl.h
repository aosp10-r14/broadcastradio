/*=============================================================================
    start of file
=============================================================================*/

/*******************************************************************************
 * Copyright (c) 2019-2020 OpenSynergy GmbH.
 *
 * This software may not be used in any way or distributed without
 * permission. All rights reserved.
 ******************************************************************************/

/**
 *@file      tunsdk.h $RelUrl: trunk/inc/sdk01/tunsdk_tpl.h $
 *@version   $LastChangedRevision: 10559 $
 *
 *@brief This file contains CTUNSDK_Tpl
 *
 */

#ifndef TUNSDK_TPL_H_
#define TUNSDK_TPL_H_

#include "tunsdk_types.h"

#define TUNSDK_TPL_MAX_MESSAGES 13U
#define TUNSDK_TPL_MAX_MESSAGE_LENGTH 512U
#define TUNSDK_TPL_MESSAGE_HEADER 128U
#define TUNSDK_TPL_MESSAGE_BUFFER_LENGTH (TUNSDK_TPL_MESSAGE_HEADER+(TUNSDK_TPL_MAX_MESSAGES*TUNSDK_TPL_MAX_MESSAGE_LENGTH))

/**
 * @brief a reference to a TPL message under construction. Only construct one message at a time!
 * Only use the TUNSDK_TPL_Multi returned to you from this API. Clients shall not create their own TUNSDK_TPL_Multi
 */
typedef  struct
{
    uint8_t (*const data) [TUNSDK_TPL_MESSAGE_BUFFER_LENGTH]; /**<the single one location in which TPL messages can be crafted*/
}TUNSDK_TPL_Multi;

/**
 * @brief a command header for Tpl communication
 */
typedef struct
{
    uint8_t protocol;              /**< Protocol to use for that command */
    uint8_t deviceId;              /**< Device to send the command to */
    uint8_t priority;              /**< Priority of the command */
}Ts_tunsdk_tpl_command_header;

/**
 *@brief This class provides static helper functions for interacting with a TPL through #CTUNSDK_ClientBase::TUNSDK_TplSend
 */
class CTUNSDK_Tpl
{
private:
    /**
     *@brief You cannot and need not instantiate CTUNSDK_Tpl
     */
    CTUNSDK_Tpl(void);
    ~CTUNSDK_Tpl(void);

public:
    static const uint8_t TPL_MERCURY_DEVICE_ID_AUDIO = 2U; /**<use to configure #Ts_tunsdk_tpl_command_header::deviceId to sned messages to audio*/
    /**
     * Create one single command
     * @param header
     * @param buffer
     * @param length
     * @param rd_length
     * @return
     */
    static TUNSDK_TPL_Multi TUNSDK_Tpl_SingleCommand(const Ts_tunsdk_tpl_command_header& header, const uint8_t* buffer, uint16_t length, uint16_t rd_length);

    /**
     * Allocate one multi command to which you can add commands later before sending
     * @param header
     * @return
     */
    static TUNSDK_TPL_Multi TUNSDK_Tpl_MultiAlloc(const Ts_tunsdk_tpl_command_header& header);

    /**
     * add one command to a multi command
     * @param multi the mutli command
     * @param buffer the buffer containing the command to add
     * @param length the length of the command
     * @param readLength the length of the read operation to do after this command
     * @return #TUNSDK_STATUS_OK in case the command could be added successfully
     */
    static Te_tunsdk_status TUNSDK_Tpl_MultiAdd(TUNSDK_TPL_Multi multi, const uint8_t* buffer, uint16_t length, uint16_t readLength);

    /**
     * Convert a multi (or single) command to #Ts_tunsdk_rawBuf
     * @param multi parameter to convert
     * @return the conversion result
     */
    static Ts_tunsdk_rawBuf TUNSDK_Tpl_MultiAsRaw(TUNSDK_TPL_Multi multi);

    /**
     * Gets the number of messages contained. Note that a message can be empty (size 0)
     * @param buf buffer to use
     * @return the number of messages, including empty messages
     */
    static uint8_t TUNSDK_Tpl_GetNrMessages(Ts_tunsdk_rawBuf buf);

    /**
     * Get the n-th message from the raw buffer. Note that some messages can be empty. Anyway, non-empty messages can be there after that
     * @param buf buffer to use
     * @param idx the index to use
     * @param length the length of the message. Check this == 0 to see whether message was empty
     * @param readLength the readLength for this message
     * @return the message, or null if there's no such message
     */
    static uint8_t* TUNSDK_Tpl_GetMessage(Ts_tunsdk_rawBuf buf, uint8_t idx, uint16_t& length, uint16_t& readLength);

    /**
     * Get the command header from a raw buf
     * @param buf the buffer to use
     * @return the command header in this buffer
     */
    static Ts_tunsdk_tpl_command_header TUNSDK_Tpl_GetHeader(Ts_tunsdk_rawBuf buf);
};

#endif/* TUNSDK_TPL_H_*/
