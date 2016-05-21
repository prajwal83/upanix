#	 Upanix - An x86 based Operating System
#	 Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
#																			 
#	 This program is free software: you can redistribute it and/or modify
#	 it under the terms of the GNU General Public License as published by
#	 the Free Software Foundation, either version 3 of the License, or
#	 (at your option) any later version.
#																			 
#	 This program is distributed in the hope that it will be useful,
#	 but WITHOUT ANY WARRANTY; without even the implied warranty of
#	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	 GNU General Public License for more details.
#																			 
#	 You should have received a copy of the GNU General Public License
#	 along with this program.  If not, see <http://www.gnu.org/licenses/
. setup.sh

echo rahasya | sudo -S mount floppy/GrubFloppy_ext.img floppy/MntFloppy -o loop 

if [ $? -ne 0 ]
then
exit 100
fi

cp -f bin/upanix.elf floppy/MntFloppy/boot

echo rahasya | sudo -S umount floppy/MntFloppy 

echo rahasya | sudo -S kpartx -d /dev/sdb
echo rahasya | sudo -S kpartx -av /dev/sdb
echo rahasya | sudo -S mount /dev/mapper/sdb1 usb_boot/MntUSB 

#echo rahasya | sudo -S kpartx -d USBImage/300MUSB.img
#echo rahasya | sudo -S kpartx -av USBImage/300MUSB.img
#echo rahasya | sudo -S mount /dev/mapper/loop0p1 usb_boot/MntUSB 

if [ $? -ne 0 ]
then
  exit 100
fi

echo rahasya | sudo -S cp -f bin/upanix.elf usb_boot/MntUSB/efi/boot/

echo rahasya | sudo -S umount usb_boot/MntUSB

echo "installed..."
