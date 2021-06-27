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

#ifndef Queue_hpp
#define Queue_hpp

#include <list>
#include <mutex>
#include <condition_variable>
#include <cstdio>

class PacketItem
{
public:
    PacketItem(const std::vector<char> packet, const int type, const int tag)
        :
        m_packet{packet},
        m_type{type},
        m_tag{tag}
    {
    }

    std::vector<char> getPacket() const noexcept { return m_packet; }
    int getType() const noexcept { return m_type; }
    int getTag() const noexcept { return m_tag; }

private:
    std::vector<char> m_packet;
    int m_type;
    int m_tag;
};

template <typename T>
class WorkQueue
{
public:
    WorkQueue() = default;
    ~WorkQueue() = default;

    void add(T item)
    {
        m_mutex.lock();
        m_queue.push_back(item);
        m_cv.notify_all();
        m_mutex.unlock();
    }

    T remove()
    {
        std::unique_lock<std::mutex> lock{m_mutex};
        m_cv.wait(lock, [&](){ return m_shouldStop || !m_queue.empty(); });

        if (m_queue.size() > 0)
        {
            auto item = m_queue.front();
            m_queue.pop_front();
            lock.unlock();
            return item;
        }
        else
        {
            lock.unlock();
            printf("No item to remove. item count: %d\n", this->size());
            return nullptr;
        }
    }

    std::size_t size()
    {
        m_mutex.lock();
        const auto size = m_queue.size();
        m_mutex.unlock();
        return size;
    }

    void stop()
    {
        m_shouldStop = true;
        m_cv.notify_all();
    }

private:
    // The Queue that will contain the work items
    std::list<T> m_queue;

    // The mutext that we'll use to lock access to the queue
    std::mutex m_mutex;

    // The Condition variable that determines when remove() can stop waiting
    // because there is either a new item in the queue or the queue should stop.
    std::condition_variable m_cv;

    // Whether the queue should stop. This is required becuase there was an
    // deadlock issue where trying to stop an std::thread that is waiting on a
    // std::condition_variable to return. This stackoverflow question explains
    // in more detail https://stackoverflow.com/q/21757124
    bool m_shouldStop{false};
};

#endif
