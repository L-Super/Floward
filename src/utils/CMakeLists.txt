target_sources(${PROJECT_NAME} PUBLIC
        Config.cpp Config.h
        AutoStartup.cpp AutoStartup.h
        Util.cpp Util.h
        Logger.hpp
        Expected.hpp
)

if (APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
            ClipboardPollHelper.h
            ClipboardPollHelper.mm
    )
endif ()