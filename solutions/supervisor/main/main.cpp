#include <cctype> // for isdigit()
#include <cstdio>
#include <cstdlib> // for exit()
#include <signal.h>
#include <string>
#include <sys/stat.h> // for umask()
#include <unistd.h> // for fork()

#include "http_server.h"

// Default configuration
#define DEFAULT_HTTP_PORT "80"
#define DEFAULT_ROOT_DIR "/usr/share/supervisor/www/"
#define DEFAULT_SCRIPT_PATH "/usr/share/supervisor/scripts/main.sh"

void print_help(char* argv0)
{
    fprintf(stderr, "Usage: %s [-v] [-B] [-r root_dir] [-p port] [-s script_path] [-a] [-D log_level]\n", argv0);
    fprintf(stderr, "  -v: Show version\n");
    fprintf(stderr, "  -B: Run as daemon\n");
    fprintf(stderr, "  -r: Set root directory (default: %s)\n", DEFAULT_ROOT_DIR);
    fprintf(stderr, "  -p: Set HTTP port (default: %s)\n", DEFAULT_HTTP_PORT);
    fprintf(stderr, "  -s: Set script path (default: %s)\n", DEFAULT_SCRIPT_PATH);
    fprintf(stderr, "  -a: Disable authentication\n");
}

int main(int argc, char** argv)
{
    // Initialize configuration parameters
    std::string root_dir = DEFAULT_ROOT_DIR;
    std::string http_port = DEFAULT_HTTP_PORT;
    std::string script_path = DEFAULT_SCRIPT_PATH;
    uint8_t log_level = LOG_WARNING;
    bool no_auth = false;
    bool daemon_mode = false;
    bool show_version = false;

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "vBr:p:s:aD:")) != -1) {
        switch (opt) {
        case 'v':
            show_version = true;
            break;
        case 'B':
            daemon_mode = true;
            break;
        case 'r':
            root_dir = optarg;
            break;
        case 'p':
            http_port = optarg;
            break;
        case 's':
            script_path = optarg;
            break;
        case 'a':
            no_auth = true;
            break;
        case 'D':
            log_level = atoi(optarg);
            break;
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // If daemon mode is enabled
    if (daemon_mode) {
        // Create child process
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork\n");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // Parent process exits
            exit(EXIT_SUCCESS);
        }

        // Child process continues, becoming session leader
        if (setsid() < 0) {
            fprintf(stderr, "Failed to setsid\n");
            exit(EXIT_FAILURE);
        }

        // Redirect standard input, output, and error
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Set file permission mask
        umask(0);
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT); // CTRL+C
    sigaddset(&sigset, SIGTSTP); // CTRL+Z
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    Logger logger(PROJECT_NAME, log_level);

    try {
        if (show_version) {
            fprintf(stdout, "Build Time: %s %s\n", __DATE__, __TIME__);
            fprintf(stdout, "%s V%s\n", PROJECT_NAME, PROJECT_VERSION);
        }

        // Apply configuration parameters
        api_base::set_force_no_auth(no_auth);
        api_base::set_script(script_path);

        http_server server(root_dir);
        if (!server.start(http_port)) {
            LOGE("Failed: server.start()");
        } else {
            int sig;
            sigwait(&sigset, &sig); // block until signal received
            LOGW("Exited with sig: %d", sig);
        }
    } catch (std::exception& e) {
        LOGE("Exception: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception");
    }
    LOGV("");

    return 0;
}