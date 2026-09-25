#pragma once
#include <mutex>
// Serialize configuration changes with capture/save/convert. Acquisition raw
// publishing does not need this lock and keeps running during setup.
std::recursive_mutex &oxygen_configuration_mutex();
