/**************************************************************************/
/*  anticheat_types.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

enum AnticheatError {
	ANTICHEAT_OK = 0,
	ANTICHEAT_ERR_UNAVAILABLE = 1,
	ANTICHEAT_ERR_INIT = 2,
	ANTICHEAT_ERR_CONNECT = 3,
	ANTICHEAT_ERR_SIGNATURE = 4,
};
