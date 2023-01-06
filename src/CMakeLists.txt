project(downcast LANGUAGES C CXX)

add_executable(${PROJECT_NAME}
    WIN32
    main.cpp
    ui.h
    ui.cpp
    MutexedData.h
    SharedMutexedData.h
    storage.h
    storage.h
    storage.cpp
    sqliteengine.h
    sqliteengine_t.h
    sqliteengine.cpp
    rssparser.h
    rssparser.cpp
    httpengine.h
    httpengine.cpp
    applogic.h
    applogic.cpp

    resources/AppxManifest.xml
    resources/downcast.rc
)
target_link_libraries (${PROJECT_NAME} 
    PRIVATE
      3rdparties
      stdc++fs
      fmt::fmt-header-only
)

add_custom_target(
  APP_PATH
  COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:${PROJECT_NAME}>"
  VERBATIM
)

code_sign_target_files(${PROJECT_NAME})

pack_target(${PROJECT_NAME} )
