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
#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include <Global.h>

class Mutex
{
	private:
		__volatile__ int m_iLock ;
		__volatile__ int m_iID ;
		
		static const int FREE_MUTEX = -999 ;

	public:
		Mutex() ;
		~Mutex() ;

		bool Lock(bool bBlock = true) ;
		bool UnLock() ;

	private:

		void Acquire() ;
		void Release() ;

	friend class TestSuite ;
} ;

class Atomic
{
	public:
		static int Swap(__volatile__ int& iLock, int val) ;
} ;

#endif
