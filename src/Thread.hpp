/*
 obs-iDevice-cam-source
 Copyright (C) 2018 Will Townsend <will@townsend.io>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program. If not, see <https://www.gnu.org/licenses/>
 */

#ifndef Thread_hpp
#define Thread_hpp

#include <thread>
#include <atomic>

class Thread
{
public:
    Thread();
    virtual ~Thread();

    void start();
    void join();

    std::thread self();

    virtual void *run() = 0;

    bool isStopped() { return m_isStopped.load(); }

private:
    std::thread*        m_thread{nullptr};
    std::atomic_bool    m_isRunning{false};
    std::atomic_bool    m_isStopped{false};
};

#endif /* Thread_hpp */
