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
#include <stdio.h>
#include <EthernetRecvPacket.h>
#include <ARPHandler.h>
#include <ARPRecvPacket.h>
#include <ARPSendPacket.h>
#include <EthernetHandler.h>
#include <NetworkDevice.h>

ARPHandler::ARPHandler(EthernetHandler &ethernetHandler) : _ethernetHandler(ethernetHandler) {
}

void ARPHandler::Process(const EthernetRecvPacket& packet) {
  printf("\n Handling ARP packet");
  ARPRecvPacket arpPacket(packet);
  arpPacket.Print();
}

void ARPHandler::SendRequestForMAC(const uint8_t* targetIPAddr) {
  const uint8_t spa[] = { 0, 0, 0, 0 };
  const uint8_t tha[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  ARPSendPacket arpSendPacket(1, EtherType::IPV4,
                              NetworkPacket::MAC_ADDR_LEN, NetworkPacket::IPV4_ADDR_LEN, 1,
                              _ethernetHandler.GetNetworkDevice().GetMacAddress(),
                              spa, tha, targetIPAddr);
  _ethernetHandler.SendPacket(arpSendPacket, EtherType::ARP, tha);
}