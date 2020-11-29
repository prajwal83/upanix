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
#include <UDP4Handler.h>
#include <IPV4Handler.h>
#include <IPV4RecvPacket.h>
#include <UDP4RecvPacket.h>

UDP4Handler::UDP4Handler(IPV4Handler &ipv4Handler) : _ipv4Handler(ipv4Handler) {
}

void UDP4Handler::Process(const IPV4RecvPacket& packet) {
  printf("\n Handling UDP Packet");
  UDP4RecvPacket udpPacket(packet);
  udpPacket.Print();
}