cmake_minimum_required(VERSION 3.16)

# Build da Vita:   cmake -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
# Build de PC:     cmake -B build-pc   (precisa de libsdl2-dev; usado para testes e capturas)
if(DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND BUILD_VITA)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain")
endif()

project(RiseHalfMoon CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VITA_APP_NAME "Rise of the Half Moon")
set(VITA_TITLEID  "RHMOON001")
set(VITA_VERSION  "01.00")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(APPEND SOURCES third_party/stb_impl.c)

# Versão mostrada no "About": sobe 0,01 a cada compilação (build_number.txt guarda a contagem).
set(RHM_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_target(rhm_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${RHM_GEN_DIR}/version.h
          -P ${CMAKE_SOURCE_DIR}/tools/bump_version.cmake
  BYPRODUCTS ${RHM_GEN_DIR}/version.h
  COMMENT "A incrementar o numero de compilacao")

if(VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -fno-rtti")

  # Deixa espaço no fim do segmento de código para os dados SCE do vita-elf-create
  # (sem isto a conversão falha quando o código acaba perto de uma página).
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=0x10000")
  find_package(SDL2 REQUIRED)
  add_executable(${PROJECT_NAME} ${SOURCES})
  add_dependencies(${PROJECT_NAME} rhm_version)
  target_include_directories(${PROJECT_NAME} PRIVATE src third_party ${RHM_GEN_DIR})
  target_compile_definitions(${PROJECT_NAME} PRIVATE RHM_VITA=1 RHM_HAVE_CURL=1)
  target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2-static
    curl mbedtls mbedx509 mbedcrypto zstd z
    SceNet_stub SceNetCtl_stub SceSysmodule_stub SceAppMgr_stub SceAppUtil_stub SceAvPlayer_stub SceGxm_stub
    SceIofilemgr_stub SceRtc_stub SceShellSvc_stub SceKernelThreadMgr_stub
    pthread m)

  vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
  vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
    FILE assets assets
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g -Wall")
  add_executable(${PROJECT_NAME} ${SOURCES})
  add_dependencies(${PROJECT_NAME} rhm_version)
  target_include_directories(${PROJECT_NAME} PRIVATE src third_party ${RHM_GEN_DIR} ${SDL2_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} pthread m)
endif()
