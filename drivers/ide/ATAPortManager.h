/*
 *	Upanix - An x86 based Operating System
 *  Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
 *                                                                          
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *                                                                          
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                          
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/
 */
#ifndef _ATA_PORT_MANAGER_H_
#define _ATA_PORT_MANAGER_H_

#include <ATADeviceController.h>

#define ATAPortManager_SUCCESS		0
#define ATAPortManager_ERR_TIMEOUT	1
#define ATAPortManager_FAILURE		2

void ATAPortManager_Probe(ATAPort* pPort) ;
byte ATAPortManager_ConfigureDrive(ATAPort* pPort) ;
byte ATAPortManager_IOWait(ATAPort* pPort, unsigned uiMask, unsigned uiValue) ;
byte ATAPortManager_IOWaitAlt(ATAPort* pPort, unsigned uiMask, unsigned uiValue) ;
byte ATAPortManager_IORead(ATAPort* pPort, void* pBuffer, unsigned uiLength) ;
byte ATAPortManager_IOWrite(ATAPort* pPort, void* pBuffer, unsigned uiLength) ;

#endif
