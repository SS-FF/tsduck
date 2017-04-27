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
//
//  Representation of a Service Description Table (SDT)
//
//----------------------------------------------------------------------------

#include "tsSDT.h"
#include "tsStringUtils.h"



//----------------------------------------------------------------------------
// Default constructor:
//----------------------------------------------------------------------------

ts::SDT::SDT(bool is_actual_,
               uint8_t version_,
               bool is_current_,
               uint16_t ts_id_,
               uint16_t onetw_id_) :
    AbstractLongTable(TID(is_actual_ ? TID_SDT_ACT : TID_SDT_OTH), version_, is_current_),
    ts_id(ts_id_),
    onetw_id(onetw_id_),
    services()
{
    _is_valid = true;
}


//----------------------------------------------------------------------------
// Constructor from a binary table
//----------------------------------------------------------------------------

ts::SDT::SDT(const BinaryTable& table) :
    AbstractLongTable(TID_SDT_ACT),  // updated by Deserialize
    ts_id(0),
    onetw_id(0),
    services()
{
    deserialize(table);
}


//----------------------------------------------------------------------------
// Search a service by name.
// If the service is found, return true and set service_id. Return
// false if not found.
// If exact_match is true, the service name must be exactly identical
// to name. If it is false, the search is case-insensitive and blanks
// are ignored.
//----------------------------------------------------------------------------

bool ts::SDT::findService (const std::string& name, uint16_t& service_id, bool exact_match) const
{
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        const std::string service_name (it->second.serviceName());
        if ((exact_match && service_name == name) || (!exact_match && SimilarStrings (service_name, name))) {
            service_id = it->first;
            return true;
        }
    }

    // Service not found
    service_id = 0;
    return false;
}

bool ts::SDT::findService(ts::Service& service, bool exact_match) const
{
    uint16_t service_id = 0;
    if (!service.hasName() || !findService(service.getName(), service_id, exact_match)) {
        return false;
    }
    else {
        service.setId(service_id);
        return true;
    }
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::SDT::deserialize (const BinaryTable& table)
{
    // Clear table content
    _is_valid = false;
    ts_id = 0;
    onetw_id = 0;
    services.clear();

    if (!table.isValid()) {
        return;
    }

    // Check table id: SDT Actual or Other
    if ((_table_id = table.tableId()) != TID_SDT_ACT && _table_id != TID_SDT_OTH) {
        return;
    }

    // Loop on all sections
    for (size_t si = 0; si < table.sectionCount(); ++si) {

        // Reference to current section
        const Section& sect (*table.sectionAt(si));

        // Abort if not expected table
        if (sect.tableId() != _table_id) {
            return;
        }

        // Get common properties (should be identical in all sections)
        version = sect.version();
        is_current = sect.isCurrent();
        ts_id = sect.tableIdExtension();

        // Analyze the section payload:
        const uint8_t* data (sect.payload());
        size_t remain (sect.payloadSize());

        // Get original_network_id (should be identical on all sections).
        // Note that there is one trailing reserved byte.
        if (remain < 3) {
            return;
        }
        onetw_id = GetUInt16 (data);
        data += 3;
        remain -= 3;

        // Get services description
        while (remain >= 5) {
            uint16_t service_id = GetUInt16 (data);
            Service& serv (services[service_id]);
            serv.EITs_present = (data[2] & 0x02) != 0;
            serv.EITpf_present = (data[2] & 0x01) != 0;
            serv.running_status = data[3] >> 5;
            serv.CA_controlled = (data[3] & 0x10) != 0;
            size_t info_length (GetUInt16 (data + 3) & 0x0FFF);
            data += 5;
            remain -= 5;
            info_length = std::min (info_length, remain);
            serv.descs.add (data, info_length);
            data += info_length;
            remain -= info_length;
        }
    }

    _is_valid = true;
}


//----------------------------------------------------------------------------
// Private method: Add a new section to a table being serialized.
// Session number is incremented. Data and remain are reinitialized.
//----------------------------------------------------------------------------

void ts::SDT::addSection (BinaryTable& table,
                            int& section_number,
                            uint8_t* payload,
                            uint8_t*& data,
                            size_t& remain) const
{
    table.addSection (new Section (_table_id,
                                   true,    // is_private_section
                                   ts_id,   // tid_ext
                                   version,
                                   is_current,
                                   uint8_t(section_number),
                                   uint8_t(section_number),   //last_section_number
                                   payload,
                                   data - payload)); // payload_size,

    // Reinitialize pointers.
    // Restart after constant part of payload (3 bytes).
    remain += data - payload - 3;
    data = payload + 3;
    section_number++;
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::SDT::serialize (BinaryTable& table) const
{
    // Reinitialize table object
    table.clear();

    // Return an empty table if not valid
    if (!_is_valid) {
        return;
    }

    // Build the sections
    uint8_t payload [MAX_PSI_LONG_SECTION_PAYLOAD_SIZE];
    int section_number (0);
    uint8_t* data (payload);
    size_t remain (sizeof(payload));

    // Add original_network_id and one reserved byte at beginning of the
    // payload (will remain identical in all sections).
    PutUInt16 (data, onetw_id);
    data[2] = 0xFF;
    data += 3;
    remain -= 3;

    // Add all services
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {

        const uint16_t service_id (it->first);
        const Service& serv (it->second);

        // If we cannot at least add the fixed part, open a new section
        if (remain < 5) {
            addSection (table, section_number, payload, data, remain);
        }

        // Insert the characteristics of the service. When the section is
        // not large enough to hold the entire descriptor list, open a
        // new section for the rest of the descriptors. In that case, the
        // common properties of the service must be repeated.
        bool starting (true);
        size_t start_index (0);

        while (starting || start_index < serv.descs.count()) {

            // If we are at the beginning of a service description,
            // make sure that the entire service description fits in
            // the section. If it does not fit, start a new section.
            // Note that huge service descriptions may not fit into
            // one section. In that case, the service description
            // will span two sections later.
            if (starting && 5 + serv.descs.binarySize() > remain) {
                addSection (table, section_number, payload, data, remain);
            }

            starting = false;

            // Insert common characteristics of the service
            assert (remain >= 5);
            PutUInt16 (data, service_id);
            data[2] = 0xFC | (serv.EITs_present ? 0x02 : 0x00) | (serv.EITpf_present ? 0x01 : 0x00);
            data += 3;
            remain -= 3;

            // Insert descriptors (all or some).
            uint8_t* flags (data);
            start_index = serv.descs.lengthSerialize (data, remain, start_index);

            // The following fields are inserted in the 4 "reserved" bits
            // of the descriptor_loop_length.
            flags[0] = (flags[0] & 0x0F) | (serv.running_status << 5) | (serv.CA_controlled ? 0x10 : 0x00);

            // If not all descriptors were written, the section is full.
            // Open a new one and continue with this service.
            if (start_index < serv.descs.count()) {
                addSection (table, section_number, payload, data, remain);
            }
        }
    }

    // Add partial section (if there is one)
    if (data > payload + 3 || table.sectionCount() == 0) {
        addSection (table, section_number, payload, data, remain);
    }
}
    

//----------------------------------------------------------------------------
// Default constructor for SDT::Service
//----------------------------------------------------------------------------

ts::SDT::Service::Service () :
    EITs_present (false),
    EITpf_present (false),
    running_status (0),
    CA_controlled (false),
    descs ()
{
}


//----------------------------------------------------------------------------
// Return the service type, service name and provider name (all found from
// the first DVB "service descriptor", if there is one in the list).
//----------------------------------------------------------------------------

uint8_t ts::SDT::Service::serviceType () const
{
    // Locate the service descriptor
    const size_t index (descs.search (DID_SERVICE));

    if (index >= descs.count() || descs[index]->payloadSize() < 1) {
        return 0; // "reserved" service_type value
    }
    else {
        return descs[index]->payload()[0];
    }
}

std::string ts::SDT::Service::providerName () const
{
    // Locate the service descriptor
    const size_t index (descs.search (DID_SERVICE));
    size_t size;

    if (index >= descs.count() || (size = descs[index]->payloadSize()) < 2) {
        return "";
    }
    else {
        const uint8_t* data (descs[index]->payload());
        size_t length (std::min (size_t (data[1]), size - 2));
        return std::string (reinterpret_cast<const char*> (data + 2), length);
    }
}

std::string ts::SDT::Service::serviceName () const
{
    // Locate the service descriptor
    const size_t index (descs.search (DID_SERVICE));
    size_t size;

    if (index >= descs.count() || (size = descs[index]->payloadSize()) < 2) {
        return "";
    }
    else {
        // First skip service provider name
        const uint8_t* data (descs[index]->payload());
        size_t length (std::min (size_t (data[1]), size - 2));
        data += 2 + length;
        size -= 2 + length;
        // Then extract the service name
        if (size <= 0) {
            return "";
        }
        else {
            length = std::min (size_t (data[0]), size - 1);
            return std::string (reinterpret_cast<const char*> (data + 1), length);
        }
    }
}


//----------------------------------------------------------------------------
// Modify the service_descriptor with the new name.
//----------------------------------------------------------------------------

void ts::SDT::Service::setName (const std::string& name, uint8_t service_type)
{
    // Locate the service descriptor
    const size_t index (descs.search (DID_SERVICE));

    if (index >= descs.count() || descs[index]->payloadSize() < 2) {
        // No valid service_descriptor, add a new one.
        ByteBlock data (5);
        data[0] = DID_SERVICE; // tag
        data[1] = uint8_t (3 + name.length()); // descriptor length
        data[2] = service_type;
        data[3] = 0; // provider name length
        data[4] = uint8_t (name.length());
        data.append (name.c_str(), name.length());
        descs.add (DescriptorPtr (new Descriptor (data)));
    }
    else {
        // Replace service name in existing descriptor
        const uint8_t* payload (descs[index]->payload());
        size_t payload_size (descs[index]->payloadSize());
        size_t provider_length (std::min (size_t (payload[1]), payload_size - 2));
        ByteBlock new_payload (payload, 2 + provider_length);
        new_payload.push_back (uint8_t (name.length()));
        new_payload.append (name.c_str(), name.length());
        descs[index]->replacePayload (new_payload);
    }
}


//----------------------------------------------------------------------------
// Modify the service_descriptor with the new provider name.
//----------------------------------------------------------------------------

void ts::SDT::Service::setProvider (const std::string& provider, uint8_t service_type)
{
    // Locate the service descriptor
    const size_t index = descs.search (DID_SERVICE);
    const uint8_t* payload = index >= descs.count() ? 0 : descs[index]->payload();
    const size_t payload_size = index >= descs.count() ? 0 : descs[index]->payloadSize();

    // Locate existing service type
    if (payload_size >= 1) {
        service_type = payload[0];
    }

    // Locate existing service name
    const uint8_t* name = 0;
    size_t name_size = 0;
    if (payload_size >= 2) {
        size_t provider_size = payload[1];
        if (2 + provider_size + 1 <= payload_size) {
            name_size = std::min (size_t (payload [2 + provider_size]), payload_size - 2 - provider_size - 1);
            name = payload + 2 + provider_size + 1;
        }
    }

    if (payload_size == 0) {
        // No valid service_descriptor, add a new one.
        ByteBlock data (4);
        data[0] = DID_SERVICE; // tag
        data[1] = uint8_t (3 + provider.length()); // descriptor length
        data[2] = service_type;
        data[3] = uint8_t (provider.length());
        data.append (provider.c_str(), provider.length());
        data.appendUInt8 (0); // service name length
        descs.add (DescriptorPtr (new Descriptor (data)));
    }
    else {
        // Replace provider name in existing descriptor
        ByteBlock new_payload (2);
        new_payload[0] = service_type;
        new_payload[1] = uint8_t (provider.length());
        new_payload.append (provider.c_str(), provider.length());
        new_payload.appendUInt8 (uint8_t (name_size));
        if (name_size > 0) {
            new_payload.append (name, name_size);
        }
        descs[index]->replacePayload (new_payload);
    }
}


//----------------------------------------------------------------------------
// Modify the service_descriptor with the new service type.
//----------------------------------------------------------------------------

void ts::SDT::Service::setType (uint8_t service_type)
{
    // Locate the service descriptor
    const size_t index (descs.search (DID_SERVICE));
    size_t size;

    if (index >= descs.count() || (size = descs[index]->payloadSize()) < 2) {
        // No valid service_descriptor, add a new one.
        ByteBlock data (5);
        data[0] = DID_SERVICE;  // tag
        data[1] = 3;            // descriptor length
        data[2] = service_type;
        data[3] = 0;            // provider name length
        data[4] = 0;            // service name length
        descs.add (DescriptorPtr (new Descriptor (data)));
    }
    else if (descs[index]->payloadSize() > 0) {
        // Replace service type in existing descriptor
        uint8_t* payload (descs[index]->payload());
        payload[0] = service_type;
    }
}