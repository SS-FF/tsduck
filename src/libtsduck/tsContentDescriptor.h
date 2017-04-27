//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2017, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  Representation of a content_descriptor
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsAbstractDescriptor.h"

namespace ts {

    class TSDUCKDLL ContentDescriptor : public AbstractDescriptor
    {
    public:
        // Content entry
        struct Entry;
        typedef std::list<Entry> EntryList;

        // Public members:
        EntryList entries;

        // Default constructor
        ContentDescriptor();

        // Constructor from a binary descriptor
        ContentDescriptor (const Descriptor&);

        // Inherited methods
        virtual void serialize (Descriptor&) const;
        virtual void deserialize (const Descriptor&);

        // Content entry
        struct TSDUCKDLL Entry
        {
            // Public members
            uint8_t content_nibble_level_1;
            uint8_t content_nibble_level_2;
            uint8_t user_nibble_1;
            uint8_t user_nibble_2;

            // Contructors
            Entry (uint16_t all = 0) :
                content_nibble_level_1 (uint8_t (all >> 12) & 0x0F),
                content_nibble_level_2 (uint8_t (all >> 8) & 0x0F),
                user_nibble_1 (uint8_t (all >> 4) & 0x0F),
                user_nibble_2 (uint8_t (all) & 0x0F)
            {
            }
            Entry (uint8_t l1, uint8_t l2, uint8_t u1, uint8_t u2) :
                content_nibble_level_1 (l1),
                content_nibble_level_2 (l2),
                user_nibble_1 (u1),
                user_nibble_2 (u2)
            {
            }
        };
    };
}