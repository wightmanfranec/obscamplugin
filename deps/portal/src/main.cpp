/*
portal
Copyright (C) 2018	Will Townsend <will@townsend.io>

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

#include <chrono>
#include <cstdlib>
#include <csignal>
#include "Portal.hpp"

bool m_running{true};

int main()
{
    // Register signal handler for SIGINT
    signal(SIGINT, [](auto sig){ m_running = false; });

    portal_log_stdout("Looking for devices");

    auto client = std::make_shared<portal::Portal>();
    if (client->startListeningForDevices() != 0)
    {
        return EXIT_FAILURE;
    }

    using namespace std::chrono_literals;
    while (m_running && client->isListening())
    {
        std::this_thread::sleep_for(100ms);
    }

    portal_log_stdout("Done!");

    return EXIT_SUCCESS;
}
