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
#ifndef _XHCI_CONTROLLER_H_
#define _XHCI_CONTROLLER_H_

#include <USBController.h>
#include <PCIBusHandler.h>
#include <XHCIStructures.h>

class CommandManager;
class EventManager;

class XHCIController
{
  public:
    XHCIController(PCIEntry*);
    void Probe();

  private:
    void LoadXCaps(unsigned base);
    void PerformBiosToOSHandoff();
    USB_PROTOCOL PortProtocol(unsigned portId) const;
    const char* PortProtocolName(USB_PROTOCOL) const;
    const char* PortSpeedName(DEVICE_SPEED speed) const;

    static unsigned  _memMapBaseAddress;
    PCIEntry*        _pPCIEntry;
    XHCICapRegister* _capReg;
    XHCIOpRegister*  _opReg;
    CommandManager*  _cmdManager;
    EventManager*    _eventManager;
    LegSupXCap*      _legSupXCap;
    upan::list<SupProtocolXCap*> _supProtoXCaps;

    friend class XHCIManager;
};

class CommandManager
{
  private:
    CommandManager(XHCICapRegister&, XHCIOpRegister&);

  private:
    struct Ring
    {
      TRB _cmd;
      TRB _link;
    } PACKED;

    bool             _pcs;
    Ring*            _ring;
    XHCICapRegister& _capReg;
    XHCIOpRegister&  _opReg;
  friend class XHCIController;
};

class EventManager
{
  private:
    EventManager(XHCICapRegister&, XHCIOpRegister&);
    void DebugPrint() const;

  private:
    struct Ring
    {
      TRB _events[64];
      TRB _link;
    } PACKED;

    Ring* _ring;
    XHCICapRegister& _capReg;
    XHCIOpRegister& _opReg;
  friend class XHCIController;
};

#endif
