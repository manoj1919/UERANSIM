//
// This file is a part of UERANSIM open source project.
// Copyright (c) 2021 ALİ GÜNGÖR.
//
// The software and all associated files are licensed under GPL-3.0
// and subject to the terms and conditions defined in LICENSE file.
//

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace app
{

struct GnbCliCommand
{
    enum PR
    {
        STATUS,
        INFO,
        AMF_LIST,
        AMF_INFO,
        UE_LIST,
        UE_COUNT,
        HANDOVERPREPARE,
        HANDOVER
    } present;

    // AMF_INFO
    int amfId{};
    
    // HANDOVERPREPARE
    int ueId{};

    // HANDOVER
    int asAmfId{};
    int64_t amfUeNgapId{};
    int64_t ranUeNgapId{};
    int ctxtId{};
    int ulStr{};
    std::string amf_name{};
    
    
    explicit GnbCliCommand(PR present) : present(present)
    {
    }
};

struct UeCliCommand
{
    enum PR
    {
        INFO,
        STATUS,
        TIMERS,
        DE_REGISTER,
    } present;

    // DE_REGISTER
    bool isSwitchOff{};
    bool dueToDisable5g{};

    explicit UeCliCommand(PR present) : present(present)
    {
    }
};

std::unique_ptr<GnbCliCommand> ParseGnbCliCommand(std::vector<std::string> &&tokens, std::string &error,
                                                  std::string &output);

std::unique_ptr<UeCliCommand> ParseUeCliCommand(std::vector<std::string> &&tokens, std::string &error,
                                                std::string &output);

} // namespace app
