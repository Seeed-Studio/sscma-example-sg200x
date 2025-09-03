#include <cctype> // for isdigit()
#include <cstdio>
#include <cstdlib> // for exit()
#include <semaphore.h> // 添加信号量头文件
#include <signal.h>
#include <string>
#include <sys/stat.h> // for umask()
#include <unistd.h> // for fork()
#include <fcntl.h> // for open(), O_RDWR

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

static sem_t signal_sem;
static int signal_num;

void signal_handler(int sig)
{
    signal_num = sig;
    sem_post(&signal_sem);
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
    while ((opt = getopt(argc, argv, "hvBr:p:s:aD:")) != -1) {
        switch (opt) {
        case 'h':
            print_help(argv[0]);
            return 0;
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

    // Validate http_port: must be all digits
    if (http_port.empty() ||
        !std::all_of(http_port.begin(), http_port.end(), [](unsigned char ch) { return std::isdigit(ch); })) {
        LOGW("Invalid HTTP port '%s', falling back to default %s", http_port.c_str(), DEFAULT_HTTP_PORT);
        http_port = DEFAULT_HTTP_PORT;
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

        // Change working directory to root to avoid locking mount points
        chdir("/");

        // Redirect standard input, output, and error to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        } else {
            // Fallback: close std fds if /dev/null open fails
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }

        // Set file permission mask
        umask(0);
    }

    Logger logger(PROJECT_NAME, log_level);

    if (sem_init(&signal_sem, 0, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGTRAP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGPIPE, signal_handler);

    try {
        if (show_version) {
            fprintf(stdout, "Build Time: %s %s\n", __DATE__, __TIME__);
            fprintf(stdout, "%s V%s\n", PROJECT_NAME, PROJECT_VERSION);
            return 0;
        }

        // Apply configuration parameters
        api_base::set_force_no_auth(no_auth);
        api_base::set_script(script_path);

        http_server server(root_dir);
        if (!server.start(http_port)) {
            LOGE("Failed: server.start()");
            return EXIT_FAILURE;
        } else {
            sem_wait(&signal_sem);
            LOGW("Received signal(%d), exiting main", signal_num);
        }
    } catch (const std::exception& e) {
        LOGE("Exception: %s", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        LOGE("Unknown exception");
        return EXIT_FAILURE;
    }
    LOGV("");

    return 0;
}