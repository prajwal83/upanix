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
#include <Display.h>
#include <EHCIController.h>
#include <EHCIManager.h>

EHCIManager::EHCIManager()
{
	byte bControllerFound = false ;

	for(auto pPCIEntry : PCIBusHandler::Instance().PCIEntries())
	{
		if(pPCIEntry->bHeaderType & PCI_HEADER_BRIDGE)
			continue ;

		if(pPCIEntry->bInterface == 32 && 
			pPCIEntry->bClassCode == PCI_SERIAL_BUS && 
			pPCIEntry->bSubClass == PCI_USB)
		{
      printf("\n Interface = %u, Class = %u, SubClass = %u", pPCIEntry->bInterface, pPCIEntry->bClassCode, pPCIEntry->bSubClass);
      int memMapIndex = _controllers.size();
      _controllers.push_back(new EHCIController(pPCIEntry, memMapIndex));
			bControllerFound = true ;
		}
	}
	
	if(bControllerFound)
		KC::MDisplay().LoadMessage("USB EHCI Controller Found", Success) ;
	else
		KC::MDisplay().LoadMessage("No USB EHCI Controller Found", Failure) ;
}

void EHCIManager::ProbeDevice()
{
  for(auto c : _controllers)
    c->Probe();
}

byte EHCIManager::RouteToCompanionController()
{
  for(auto c : _controllers)
	{
		if(c->PerformBiosToOSHandoff() != EHCIController_SUCCESS)
			continue ;

		if(c->SetConfigFlag(false) != EHCIController_SUCCESS)
			continue ;
	}

	return EHCIController_SUCCESS ;
}