#pragma once

#include <string>

namespace witness {
namespace platform {

void Init();
void Shutdown();

bool ReadState(std::string& out);
bool WriteState(const std::string& data);
void ClearState();

void OpenUrl(const char* url);

bool IsWeb();

}
}
