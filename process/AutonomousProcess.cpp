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

#include <AutonomousProcess.h>
#include <Thread.h>
#include "ProcessManager.h"

AutonomousProcess::AutonomousProcess(const upan::string& name, int parentID, bool isFGProcess)
  : SchedulableProcess(name, parentID, isFGProcess), _nextThreadIt(_threadSchedulerList.begin()) {
}

SchedulableProcess& AutonomousProcess::forSchedule() {
  if (_status == TERMINATED || _status == RELEASED) {
    return *this;
  }

  if (_nextThreadIt == _threadSchedulerList.end()) {
    _nextThreadIt = _threadSchedulerList.begin();
    return *this;
  }

  while(!_threadSchedulerList.empty()) {
    if (_nextThreadIt == _threadSchedulerList.end()) {
      _nextThreadIt = _threadSchedulerList.begin();
    }
    auto curThreadIt = _nextThreadIt++;
    Thread& thread = **curThreadIt;

    if (thread.status() == RELEASED) {
      _threadSchedulerList.erase(curThreadIt);
      ProcessManager::Instance().RemoveFromProcessMap(thread);
      delete &thread;
    } else {
      return thread;
    }
  }

  return *this;
}

//TODO: use a process level lock instead of global process switch lock
void AutonomousProcess::addToThreadScheduler(Thread& thread) {
  ProcessSwitchLock lock;
  _threadSchedulerList.push_back(&thread);
  thread.setStatus(RUN);
}

void AutonomousProcess::DestroyThreads() {
  // child threads should be destroyed
  // threads must be destroyed before dealing with child processes because
  // child processes if any of a thread will be redirected to current process (main thread)
  for(auto t : _threadSchedulerList) {
    if (t->status() != TERMINATED && t->status() != RELEASED) {
      t->Destroy();
    }
    t->Release();
    ProcessManager::Instance().RemoveFromProcessMap(*t);
    delete t;
  }
  _threadSchedulerList.clear();
}