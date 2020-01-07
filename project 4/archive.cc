#include <iostream>
#include <getopt.h>
#include <pwd.h>
#include <vector>
#include <algorithm>
#include <string>
#include "archive.h"
#include "document.h"
#include "whitelist.h"
#include "thread-pool.h"
#include "ostreamlock.h"

using namespace std;

static const size_t permitThreads = 16;

/* Constructs an InternetArchive object. whitelistedHosts is a list of hosts to
 * allow; quiet specifies whether logging should be suppressed (pass this to
 * the Log class if you use it) */
InternetArchive::InternetArchive(const vector<string>& whitelistedHosts,
        bool quiet, bool memoryOnly) : index(memoryOnly) {
    for (size_t i =0; i < whitelistedHosts.size(); i++){
        wl.addHost(whitelistedHosts[i]);
    }
}

void InternetArchive::download(const string seedUrl) {
    //Only permit no more than 16 threads are actively downloading Documents at any time
    ThreadPool pool(permitThreads);
    // check url already been created
    vector<string>::iterator it = find(url_done.begin(), url_done.end(), seedUrl);
    if (it != url_done.end()) return;
    // check url accessibility
    if (!wl.canAccess(seedUrl)) return;
    Document doc = Document(seedUrl);
    url_done.push_back(seedUrl);
    try {
        doc.download();
        index.addDocument(doc);
    }
    catch(DownloadException) {
        return;
    }
    // We spawn thread to handle all outgoing links
    vector<string> links;
    links = doc.getLinks();
    const size_t NumFunctions = links.size();
    for (size_t i = 0; i < NumFunctions; i++){
        pool.schedule([=](){
            InternetArchive::download(links[i]);

        });
    }
    pool.wait();
}


// NOTE: you probably don't need to change anything below here.

void InternetArchive::serve(unsigned short port) {
    index.serve(port);
}

void InternetArchive::dumpIndex() {
    index.dumpToTerminal();
}

/* Uses the logged-in user ID to generate a port number between 2000 and
 * USHRT_MAX inclusive. We hash the logged-in user ID to a number so that users
 * can launch the proxy to listen to a port that, with very high probability,
 * no other user is likely to generate. */
static const unsigned short kLowestOpenPortNumber = 2000;
unsigned short computeDefaultPort() {
    struct passwd *pwd = getpwuid(getuid());
    if (pwd == NULL || pwd->pw_name == NULL) {
        throw runtime_error("Could not determine username on system!");
    }
    string username = pwd->pw_name;
    size_t hashValue = hash<string>()(username);
    return hashValue % (USHRT_MAX - kLowestOpenPortNumber) + kLowestOpenPortNumber;
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        {"whitelist", required_argument, NULL, 'w'},
        {"download", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"no-serve", no_argument, NULL, 'n'},
        {"dump-index", no_argument, NULL, 'i'},
        {"quiet", no_argument, NULL, 'q'},
        {"mem-only", no_argument, NULL, 'm'},
        {NULL, 0, NULL, 0},
    };

    vector<string> whitelist;
    string target;
    unsigned short port = 0;    // If not overridden, gets set after while loop
    bool dumpIndex = false;
    bool downloadOnly = false;
    bool quiet = false;
    bool memoryOnly = false;

    while (true) {
        int ch = getopt_long(argc, argv, "w:d:p:niqm", options, NULL);
        if (ch == -1) {
            break;
        } else if (ch == 'w') {
            whitelist.push_back(optarg);
        } else if (ch == 'd') {
            target = optarg;
        } else if (ch == 'p') {
            try {
                int parsed = stoi(optarg);
                if (parsed < 1 || parsed > USHRT_MAX) {
                    throw runtime_error("port number out of range");
                }
                port = parsed;
            } catch (exception& e) {
                cout << "--port argument must be a number 1-65,535" << endl;
                return 1;
            }
        } else if (ch == 'n') {
            downloadOnly = true;
        } else if (ch == 'i') {
            dumpIndex = true;
        } else if (ch == 'q') {
            quiet = true;
        } else if (ch == 'm') {
            memoryOnly = true;
        } else {
            // An error occurred.
            cout << "Usage: archive [--whitelist <domain-name>] [--download <url>] [--port <port-number>] [--no-serve]"
                << endl
                << "Examples:" << endl
                << "# no download, just serve already-downloaded documents:" << endl
                << "archive" << endl
                << "# serve on port 12345 instead of your user default:" << endl
                << "archive -p 12345" << endl
                << "# download https://www.stanford.edu, following only links at www.stanford.edu" << endl
                << "archive -w www.stanford.edu -d https://www.stanford.edu" << endl
                << "# same as above, but also follow links to carta.stanford.edu" << endl
                << "archive -w www.stanford.edu -w carta.stanford.edu -d https://www.stanford.edu" << endl
                << "# download https://www.stanford.edu, but exit after download (don't launch server)" << endl
                << "archive --no-serve -w www.stanford.edu -d https://www.stanford.edu" << endl;
            return 1;
        }
    }
    if (port == 0) port = computeDefaultPort();

    // Launch program
    InternetArchive archive(whitelist, quiet, memoryOnly);
    if (target.length()) archive.download(target);
    if (dumpIndex) archive.dumpIndex();
    if (!downloadOnly) archive.serve(port);
}
