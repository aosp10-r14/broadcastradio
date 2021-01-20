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
 *@file      tunsdk.h $RelUrl: trunk/inc/sdk01/tunsdk.h $
 *@version   $LastChangedRevision: 10370 $
 *
 *@brief This file contains the declaration of CTUNSDK.
 *
 */

#ifndef TUNSDK_H_
#define TUNSDK_H_

#include "tunsdk_types.h"
#include "tunsdk_clientbase.h"

/**
 *@brief definition of on start callback
 */
typedef void (*TUNSDK_On_StartCb)(void);

/**
 *@brief definition of the image callback
 */
typedef void (*TUNSDK_On_GetImageCb)(uint8_t* data, uint32_t length);

/**
 *@brief This class provides the static entry point for any TUNSDK client.
 */
class CTUNSDK
{
private:
    /**
     *@brief You cannot and need not instantiate CTUNSDK
     */
    CTUNSDK(void);
    ~CTUNSDK(void);

public:

    /**
     *@brief Retrieve the TUNSDK software version as a string.
     *@return 0-terminated C string identifying the software version
     */
    static const T_tunsdk_char* getVersion(void);

    /**
     *@brief This function establishes communication with the Tuner's IPC. All parameters must be non-NULL.
     *@param client your implementation of the #CTUNSDK_ClientBase handler.
     *@param cb the callback will be called when you have registered to IPC (this does not yet mean you are connected to TUNSDK!, see #CTUNSDK_ClientBase::TUNSDK_On_Connected)
     *@param name your unique client name. Note using a non-unique name results in undefined behavior
     *@par Detailed description
     *This API establishes communication with TUNSDK. In case all parameters are non-NULL, the call does not return.
     *Effectively, you are turning in the calling thread to become the TUNSDK communication thread. Make sure you call
     *this from a thread that is designed specifically for this purpose. Make sure clients use unique names, i.e. no two clients must use the same name at the same time
     *Failure to follow this rule results in undefined behavior. Either of the clients will not be able to receive messages from TUNSDK.
     *@return Normally, this function does not return. It will only return in case name or client are NULL
     */
    static Te_tunsdk_status initCommunication (const T_tunsdk_char* name, CTUNSDK_ClientBase* client, TUNSDK_On_StartCb cb);

};

#endif/* TUNSDK_H_*/
