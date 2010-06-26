/*
 *  SMSC.h
 *  FakeSMCSuperIO
 *
 *  Created by mozo on 22/06/10.
 *  Copyright 2010 mozodojo. All rights reserved.
 *
 */

#ifndef _SMSC_H
#define _SMSC_H

#include "SuperIO.h"

const UInt8 SMSC_PORTS_COUNT = 2;
const UInt16 SMSC_PORT[] = { 0x2e, 0x4e };

const UInt8 SMSC_HARDWARE_MONITOR_LDN		= 0x0a;
const UInt8 LPC47N252_HARDWARE_MONITOR_LDN	= 0x09;
const UInt8 SMSC_HARDWARE_MONITOR_LDN_VA	= 0x08;

class SMSC : public SuperIO
{
public:	
	virtual void	Enter();
	virtual void	Exit();
	
	virtual bool	ProbeCurrentPort();
	
	virtual void	Init();
	virtual void	Finish();
};

#endif