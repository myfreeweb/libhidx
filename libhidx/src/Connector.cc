#include "Connector.hh"

#include <libhidx/server/Server.hh>

#ifdef __linux__
#include "subprocess.hh"
#endif

using generic = asio::generic::stream_protocol;

#ifdef __linux__
using local = asio::local::stream_protocol;
#endif

namespace libhidx {

    LocalConnector::LocalConnector() {

    }

    bool libhidx::LocalConnector::connect() {
        return true;
    }

    std::string libhidx::LocalConnector::sendMessage(const std::string& message) {
        return server::cmd(message);
    }

#ifdef __linux__

    constexpr size_t length(const char* str)
    {
        return *str ? 1 + length(str + 1) : 0;
    }

    UnixSocketConnector::UnixSocketConnector() {
        constexpr const char* t = "/tmp/libhidxXXXXXX";
        std::string ta{t};
        std::array<char, length(t) + 1> temp;

        std::copy(std::begin(ta), std::end(ta), std::begin(temp));
        temp.back() = 0;


        mkdtemp(temp.data());
        m_socketDir = temp.data();

        auto serverPath = getServerPath();

        m_process = std::make_unique<subprocess::Popen>(
            "pkexec " + serverPath + " -p -u " + temp.data(),
            subprocess::input{subprocess::PIPE},
            subprocess::output{subprocess::PIPE}
        );

        m_ioService.run();
    }

    std::string UnixSocketConnector::getServerPath() {
        const static std::vector<std::string> possiblePaths{
            getExecutablePath() + "/../libhidx/libhidx_server_daemon",
            "/usr/local/libexec",
            "/usr/libexec"
        };
        const static std::string executableName{"libhidx_server_daemon"};

        for(const auto& dir: possiblePaths){
            auto path = dir + '/' + executableName;
            if(access(path.c_str(), X_OK) != -1){
                return path;
            }
        }

        throw IOException{"Cannot find server binary!"};
    }

    UnixSocketConnector::~UnixSocketConnector() {
        m_process->kill();
    }

    bool UnixSocketConnector::connect() {

        std::string sockPath{m_socketDir};
        sockPath += "/";
        sockPath += SOCKET_FILENAME;
        local::endpoint ep{sockPath};

        local::socket socket(m_ioService);
        socket.connect(ep);

        m_socket = std::make_unique<asio::generic::stream_protocol::socket>(std::move(socket));
        return true;
    }

    std::string UnixSocketConnector::sendMessage(const std::string& message) {
        utils::writeMessage(*m_socket, message);
        return utils::readMessage(*m_socket);
    }

    std::string UnixSocketConnector::getExecutablePath() {
        char result[ PATH_MAX ];
        ssize_t count = readlink( "/proc/self/cwd", result, PATH_MAX );
        return std::string( result, (count > 0) ? count : 0 );
    }
#endif
}
