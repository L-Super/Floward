target_sources(${PROJECT_NAME} PUBLIC
        Config.cpp Config.h
        AutoStartup.cpp AutoStartup.h
        Util.cpp Util.h
        Logger.hpp
        Expected.hpp
)

if (ENABLE_SYNC_FEATURE)
    target_sources(${PROJECT_NAME} PUBLIC
            Crypto.cpp Crypto.h
    )
endif ()

if (APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
            ClipboardPollHelper.h
            ClipboardPollHelper.mm
            UtilMac.mm
    )
endif ()