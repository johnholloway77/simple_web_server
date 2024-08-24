cmake_minimum_required(VERSION 3.5)

project(simple_server LANGUAGES C)

add_executable(simple_server main.c
    flags/flags.h
    sig_handlers/reap.h
    sig_handlers/reap.c

    sockets/createSocket_v4.c

    sockets/createSocket_v6.c
    sockets/socket.h
    sockets/handleSocket.c
    sockets/handleConnection.c
    requests/requests.h
    requests/parseRequest.c
    requests/checkMethod.c
    requests/checkHttp.c

    cgi/cgi.h
    cgi/cgiExe.c
    setup.sh
    response/response.h
    flags/setFlags.c
    response/dirResponse.h
    response/dirResponse.c

)

target_link_libraries (simple_server magic)

include(GNUInstallDirs)
install(TARGETS simple_server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
