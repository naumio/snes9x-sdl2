cmake_minimum_required(VERSION 3.1)
 
project(snes9x-sdl2)
 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -DHAVE_STDINT_H -pthread -Wl,-rpath,'$ORIGIN'")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -DHAVE_STDINT_H -pthread -Wl,-rpath,'$ORIGIN'")
 

include_directories(
    ../
    ../../
    ../filter
)
 
set(S9X_SOURCES
../apu/apu.cpp
../apu/SNES_SPC.cpp
../apu/SNES_SPC_misc.cpp
../apu/SNES_SPC_state.cpp
../apu/SPC_DSP.cpp
../apu/SPC_Filter.cpp
../bsx.cpp
../c4.cpp
../c4emu.cpp
../cheats.cpp
../cheats2.cpp
../clip.cpp
../conffile.cpp
../controls.cpp
../cpu.cpp
../cpuexec.cpp
../cpuops.cpp
../crosshairs.cpp
../dma.cpp
../dsp.cpp
../dsp1.cpp
../dsp2.cpp
../dsp3.cpp
../dsp4.cpp
../fxinst.cpp
../fxemu.cpp
../gfx.cpp
../globals.cpp
../logger.cpp
../memmap.cpp
../movie.cpp
../obc1.cpp
../ppu.cpp
../reader.cpp
../sa1.cpp
../sa1cpu.cpp
../screenshot.cpp
../sdd1.cpp
../sdd1emu.cpp
../seta.cpp
../seta010.cpp
../seta011.cpp
../seta018.cpp
../snapshot.cpp
../snes9x.cpp
../spc7110.cpp
../srtc.cpp
../tile.cpp
../filter/2xsai.cpp
../filter/blit.cpp
../filter/epx.cpp
../filter/hq2x.cpp
../filter/snes_ntsc.c
sdlmain.cpp
sdlinput.cpp
sdlvideo.cpp
sdlaudio.cpp
)
 
add_executable(snes9x-sdl2 ${S9X_SOURCES})

target_link_libraries(snes9x-sdl2
    SDL2
)