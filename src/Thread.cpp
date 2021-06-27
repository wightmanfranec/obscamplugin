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

#include "Thread.hpp"

Thread::Thread()
    :
    m_thread{nullptr},
    m_isRunning{false},
    m_isStopped{false}
{
}

Thread::~Thread()
{
    if (m_thread)
    {
        m_isStopped = true;
        if (m_isRunning && m_thread->joinable())
        {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

void Thread::start()
{
    if (!m_thread)
    {
        m_isStopped = false;
        m_thread= new std::thread([this]{ this->run(); });
        m_isRunning = true;
    }
}

void Thread::join()
{
    if (m_thread)
    {
        m_isStopped = true;

        if (m_thread->joinable())
        {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;

        m_isRunning = false;
    }
}
