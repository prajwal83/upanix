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
#include <PS2KeyboardDriver.h>
#include <Display.h>
#include <AsmUtil.h>
#include <PortCom.h>
#include <IrqManager.h>
#include <KeyboardHandler.h>
#include <KBInputHandler.h>
#include <PS2Controller.h>

static void KBDriver_Handler()
{
  AsmUtil_STORE_GPR() ;
  AsmUtil_SET_KERNEL_DATA_SEGMENTS

  //printf("\nKB IRQ\n") ;
  PS2Controller::Instance().WaitForRead();
  if(!(PortCom_ReceiveByte(PS2Controller::COMMAND_PORT) & 0x20))
    PS2KeyboardDriver::Instance().Process((PortCom_ReceiveByte(PS2Controller::DATA_PORT)) & 0xFF);

  IrqManager::Instance().SendEOI(StdIRQ::Instance().KEYBOARD_IRQ);

  AsmUtil_REVOKE_KERNEL_DATA_SEGMENTS
  AsmUtil_RESTORE_GPR() ;

  __asm__ __volatile__("LEAVE") ;
  __asm__ __volatile__("IRET") ;
}

static const byte EXTRA_KEYS = 224;
static const int MAX_KEYBOARD_CHARS = 84;

static const byte Keyboard_GENERIC_KEY_MAP[MAX_KEYBOARD_CHARS] = { Keyboard_NA_CHAR,
  Keyboard_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9',

  '0', '-', '=', Keyboard_BACKSPACE, '\t', 'q', 'w', 'e', 'r',

  't', 'y', 'u', 'i', 'o', 'p', '[', ']', Keyboard_ENTER,

  Keyboard_LEFT_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k',

  'l', ';', '\'', '`', Keyboard_LEFT_SHIFT, '\\', 'z', 'x', 'c',

  'v', 'b', 'n', 'm', ',', '.', '/', Keyboard_RIGHT_SHIFT, '*',

  Keyboard_LEFT_ALT, ' ', Keyboard_CAPS_LOCK, Keyboard_F1, Keyboard_F2,

  Keyboard_F3, Keyboard_F4, Keyboard_F5, Keyboard_F6, Keyboard_F7,

  Keyboard_F8, Keyboard_F9, Keyboard_F10, Keyboard_NA_CHAR, Keyboard_NA_CHAR, Keyboard_KEY_HOME,

  Keyboard_KEY_UP, Keyboard_KEY_PG_UP, Keyboard_NA_CHAR, Keyboard_KEY_LEFT, Keyboard_NA_CHAR,

  Keyboard_KEY_RIGHT, Keyboard_NA_CHAR, Keyboard_KEY_END, Keyboard_KEY_DOWN,

  Keyboard_KEY_PG_DOWN, Keyboard_KEY_INST, Keyboard_KEY_DEL
};

static const byte Keyboard_GENERIC_SHIFTED_KEY_MAP[MAX_KEYBOARD_CHARS] = { Keyboard_NA_CHAR,
  Keyboard_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(',

  ')', '_', '+', Keyboard_BACKSPACE, '\t', 'Q', 'W', 'E', 'R',

  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', Keyboard_ENTER,

  Keyboard_LEFT_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K',

  'L', ':', '"', '~', Keyboard_LEFT_SHIFT, '|', 'Z', 'X', 'C',

  'V', 'B', 'N', 'M', '<', '>', '?', Keyboard_RIGHT_SHIFT, '*',

  Keyboard_LEFT_ALT, ' ', Keyboard_CAPS_LOCK, Keyboard_F1, Keyboard_F2,

  Keyboard_F3, Keyboard_F4, Keyboard_F5, Keyboard_F6, Keyboard_F7,

  Keyboard_F8, Keyboard_F9, Keyboard_F10, Keyboard_NA_CHAR, Keyboard_NA_CHAR, Keyboard_KEY_HOME,

  Keyboard_KEY_UP, Keyboard_KEY_PG_UP, Keyboard_NA_CHAR, Keyboard_KEY_LEFT, Keyboard_NA_CHAR,

  Keyboard_KEY_RIGHT, Keyboard_NA_CHAR, Keyboard_KEY_END, Keyboard_KEY_DOWN,

  Keyboard_KEY_PG_DOWN, Keyboard_KEY_INST, Keyboard_KEY_DEL
};

PS2KeyboardDriver::PS2KeyboardDriver() : _isShiftKey(false), _isCapsLock(false), _isCtrlKey(false)
{
  KeyboardHandler::Instance();

  IrqManager::Instance().DisableIRQ(StdIRQ::Instance().KEYBOARD_IRQ);
  IrqManager::Instance().RegisterIRQ(StdIRQ::Instance().KEYBOARD_IRQ, (unsigned)&KBDriver_Handler);
  IrqManager::Instance().EnableIRQ(StdIRQ::Instance().KEYBOARD_IRQ);

  PortCom_ReceiveByte(PS2Controller::DATA_PORT);

  KC::MDisplay().LoadMessage("PS2 Keyboard Driver Initialization", Success);
}

void PS2KeyboardDriver::Process(byte rawKey)
{
  byte kbKey = Decode(rawKey);
  if (kbKey == Keyboard_NA_CHAR)
    return;

  if (_isCtrlKey)
    kbKey += CTRL_VALUE;

  if(!KBInputHandler_Process(kbKey))
  {
    KeyboardHandler::Instance().PutToQueueBuffer(kbKey);
    StdIRQ::Instance().KEYBOARD_IRQ.Signal();
  }
}

byte PS2KeyboardDriver::Decode(byte rawKey)
{
  if (rawKey == EXTRA_KEYS)
    return Keyboard_NA_CHAR;

  const bool keyRelease = rawKey & 0x80;
  if (keyRelease)
    rawKey -= 0x80;

  if (rawKey >= MAX_KEYBOARD_CHARS)
    return Keyboard_NA_CHAR;

  byte mappedKey = Keyboard_GENERIC_KEY_MAP[rawKey];

  if (keyRelease)
  {
    if(mappedKey == Keyboard_LEFT_SHIFT || mappedKey == Keyboard_RIGHT_SHIFT)
      _isShiftKey = false;
    else if(mappedKey == Keyboard_LEFT_CTRL)
      _isCtrlKey = false;
  }
  else
  {
    if(mappedKey == Keyboard_LEFT_SHIFT || mappedKey == Keyboard_RIGHT_SHIFT)
      _isShiftKey = true;
    else if(mappedKey == Keyboard_CAPS_LOCK)
      _isCapsLock = !_isCapsLock;
    else if(mappedKey == Keyboard_LEFT_CTRL)
      _isCtrlKey = true;
    else
    {
      if(_isShiftKey)
      {
        if(mappedKey >= 'a' && mappedKey <= 'z' && _isCapsLock)
          return mappedKey;
        return Keyboard_GENERIC_SHIFTED_KEY_MAP[rawKey];
      }
      else
      {
        if(mappedKey >= 'a' && mappedKey <= 'z' && _isCapsLock)
          return Keyboard_GENERIC_SHIFTED_KEY_MAP[rawKey];
        return mappedKey;
      }
    }
  }
  return Keyboard_NA_CHAR;
}