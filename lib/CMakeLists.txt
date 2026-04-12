# Event system library
add_library(EventSystem INTERFACE)

target_include_directories(EventSystem INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(EventSystem INTERFACE
    # Audio components
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/AudioSampleFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/FFTProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/LinkwitzRileyFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/NoteToFreq.h
    
    # Event system components
    ${CMAKE_CURRENT_SOURCE_DIR}/events/Event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/EventSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobals.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobalsListener.h
    
    # Network components
    ${CMAKE_CURRENT_SOURCE_DIR}/network/CommandBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/MulticastBroadcasterBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/SpectrumBroadcaster.h
    
    # Debug components (debug builds only)
    ${CMAKE_CURRENT_SOURCE_DIR}/debug/EditorLogger.h
)
 