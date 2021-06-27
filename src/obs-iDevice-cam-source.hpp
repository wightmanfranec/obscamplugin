/*
obs-iDevice-cam-source
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

#ifndef IDEVICESCAMSOURCE_H
#define IDEVICESCAMSOURCE_H

#include <obs-module.h>

#define blog(level, fmt, ...)                                      \
    do                                                             \
    {                                                              \
        blog(level, "[obs-ios-camera-plugin] " fmt, ##__VA_ARGS__) \
    } while (0)

#endif // IDEVICESCAMSOURCE_H
