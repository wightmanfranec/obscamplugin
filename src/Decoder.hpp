/*
 obs-iDevice-cam-source
Copyright (C) 2018-2019	Will Townsend <will@townsend.io>

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

#ifndef Decoder_hpp
#define Decoder_hpp

#include <obs.h>
#include <vector>

using Packet = std::vector<char>;

struct DecoderCallback
{
    virtual ~DecoderCallback() {}
};

class Decoder
{
protected:
    virtual ~Decoder(){};

public:
    virtual void init() = 0;
    virtual void input(const Packet packet, const int type, const int tag) = 0;
    virtual void flush() = 0;
    virtual void drain() = 0;
    virtual void shutdown() = 0;
};

#endif // Decoder_hpp
