#pragma once

#include <string>
#include <vector>
#include "index.h"
#include "whitelist.h"

class InternetArchive {
public:
    InternetArchive(const std::vector<std::string>& whitelistedHosts, bool quiet=false,
            bool memoryOnly=false);
    void download(const std::string seedUrl);
    void serve(unsigned short port);
    void dumpIndex();
    std::vector<std::string> url_done;
    Whitelist wl;

private:
    Index index;
};
