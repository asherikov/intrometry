/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

#include <intrometry/pjmsg_mcap/all.h>
#include <pjmsg_mcap_wrapper/reader.h>
#include "common.h"

#include <filesystem>


namespace intrometry_tests
{
    inline bool checkMcap(const std::filesystem::path &directory, const std::string &sink_id)
    {
        bool found_messages = false;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.path().extension() != ".mcap")
            {
                continue;
            }
            if (not sink_id.empty())
            {
                const std::string filename = entry.path().filename().string();
                if (filename.find(sink_id) == std::string::npos)
                {
                    continue;
                }
            }
            pjmsg_mcap_wrapper::Reader reader;
            reader.initialize(entry.path(), std::string("/intrometry/") + sink_id);
            pjmsg_mcap_wrapper::Message message;
            while (reader.next(message))
            {
                found_messages = true;
                if (message.names().size() != message.values().size())
                {
                    return (false);
                }
            }
        }
        return (found_messages);
    }
}  // namespace intrometry_tests
