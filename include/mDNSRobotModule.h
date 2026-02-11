//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef ROBOTMODULEINSTANCE_H
#define ROBOTMODULEINSTANCE_H

#include "flatbuffers_generated/RobotModule_generated.h"
#include <string>

struct mDNSRobotModule {
    int id;
    std::string ip;
    std::string hostname;
    ModuleType module_type;
    std::vector<int> connected_module_ids;
};

#endif // ROBOTMODULEINSTANCE_H
