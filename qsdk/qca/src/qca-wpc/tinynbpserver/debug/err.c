/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include<err.h>




char *errtostring(int err)
{
	switch(-err)
	{
		case NO_RESPONSE:
				return "No response";
		case CANT_SEND_REQ:
				return "Cant send req";
		case INVALID_RESPONSE:
				return "Invalid Reponse";
		case NO_MEMORY:
				return "No memory\n";
		case NO_FILE:
				return "No File\n";
		default:
			return "Unknown error";
	}
}
