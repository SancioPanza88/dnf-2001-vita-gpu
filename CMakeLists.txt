cmake_minimum_required(VERSION 3.10)

# DNF 2001 Vita GPU — engine riscritto da zero, rendering 100% GPU.
# Niente EDuke32, niente vita2d framebuffer software, niente memcpy CPU.
set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
project(dnf2001_gpu C)

set(TITLE_ID "DNFG20010")
set(APP_TITLE "DNF 2001 GPU")
set(APP_VERSION "01.00")

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -mcpu=cortex-a9 -mfpu=neon -ffast-math -DVITA -D__PSP2__")

include_directories(source)

add_executable(${PROJECT_NAME}
  source/main.c
  source/gpu_renderer.c
  source/grp_loader.c
  source/map_loader.c
)

target_link_libraries(${PROJECT_NAME}
  vitaGL
  SceGxm_stub
  SceDisplay_stub
  SceCtrl_stub
  ScePower_stub
  SceAudio_stub
  SceCommonDialog_stub
  m
  c
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${TITLE_ID} eboot.bin
  VERSION ${APP_VERSION}
  NAME ${APP_TITLE}
)
