cmake_minimum_required(VERSION 3.10)
project(siphon C)

set(CMAKE_C_STANDARD 11)

add_executable(siphon
        siphon.c
        proto.c
        net.c
        fs.c
        md5.c
        progress.c)
