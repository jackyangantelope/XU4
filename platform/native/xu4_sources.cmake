set(XU4_ROOT "${PORT_ROOT}/deps/xu4/src")
set(XU4_C anim.c lzw/hash.c lzw/lzw.c support/notify.c unzip.c)
set(XU4_CPP annotation aura camp cheat city codex combat controller context
  creature death debug direction discourse dungeon dungeonview error event
  filesystem game image imageloader imagemgr imageview intro item location map
  maploader menu menuitem movement names object party person portal progress_bar
  rle savegame scale screen settings shrine spell stats textview tile tileanim
  tileset tileview u4file utils view xu4 lzw/u4decode lzw/u6decode config_xml
  script_xml xml support/SymbolTable pad_native)
list(TRANSFORM XU4_CPP APPEND ".cpp")
set(XU4_SOURCES ${XU4_C} ${XU4_CPP})
list(TRANSFORM XU4_SOURCES PREPEND "${XU4_ROOT}/")
add_library(xu4_core STATIC ${XU4_SOURCES}
  "${CMAKE_CURRENT_LIST_DIR}/xu4_backend.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/xu4_audio.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/stb_vorbis.c")
set_source_files_properties("${XU4_ROOT}/xu4.cpp" PROPERTIES COMPILE_DEFINITIONS "main=xu4_game_main")
target_include_directories(xu4_core PUBLIC "${XU4_ROOT}" "${XU4_ROOT}/support" "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(xu4_core PRIVATE XU4_NATIVE=1 VERSION="1.4.3-rp2350"
  HAVE_VARIADIC_MACROS=1 STB_VORBIS_NO_PUSHDATA_API=1)
target_compile_options(xu4_core PRIVATE -fsigned-char -ffunction-sections -fdata-sections
  -Wno-write-strings -Wno-stringop-truncation -Wno-format-overflow)
