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
#include <USBKeyboard.h>
#include <stdio.h>
#include <string.h>
#include <USBController.h>
#include <USBDevice.h>

void USBKeyboardDriver::Register()
{
  USBController::Instance().RegisterDriver(new USBKeyboardDriver("USB Keyboard"));
}

int USBKeyboardDriver::MatchingInterfaceIndex(USBDevice* pUSBDevice)
{
  USBStandardConfigDesc* pConfig = &(pUSBDevice->_pArrConfigDesc[ pUSBDevice->_iConfigIndex ]) ;
  for(byte i = 0; i < pConfig->bNumInterfaces; ++i)
  {
    const auto& interface = pConfig->pInterfaces[i];
    //for now, support only keyboard with boot interface (which uses default interrupt report)
    if(interface.bInterfaceClass == CLASS_CODE && interface.bInterfaceProtocol == PROTOCOL)
      return i;
  }
  return -1;
}

bool USBKeyboardDriver::AddDevice(USBDevice* pUSBDevice)
{
  int interfaceIndex = MatchingInterfaceIndex(pUSBDevice);
  if(interfaceIndex < 0)
    return false;

  const USBStandardInterface& interface = pUSBDevice->_pArrConfigDesc[ pUSBDevice->_iConfigIndex ].pInterfaces[ interfaceIndex ];
  if(interface.bInterfaceSubClass != SUB_CLASS_CODE_BOOT)
    throw upan::exception(XLOC, "Non boot-device USB keyboard not supported - SubClass: %d", interface.bInterfaceSubClass);

  pUSBDevice->SetIdle();

  return true;
}

void USBKeyboardDriver::RemoveDevice(USBDevice* pUSBDevice)
{
}