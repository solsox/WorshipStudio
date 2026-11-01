target_sources(
  obs-studio
  PRIVATE
    docks/OBSDock.cpp
    docks/OBSDock.hpp
    docks/BibleDock.cpp
    docks/BibleDock.hpp
    docks/LiveToolsDock.cpp
    docks/LiveToolsDock.hpp
    docks/PresentationCatalogData.cpp
    docks/PresentationCatalogData.hpp
    docks/PresentationCatalogDock.cpp
    docks/PresentationCatalogDock.hpp
    docks/PresentationDock.cpp
    docks/PresentationDock.hpp
    docks/SongEditor.cpp
    docks/SongEditor.hpp
    docks/SongLibrary.cpp
    docks/SongLibrary.hpp
    docks/SongsDock.cpp
    docks/ServiceDock.cpp
    docks/ServiceDock.hpp
    docks/SongsDock.hpp
    docks/WorshipOutput.cpp
    docks/WorshipDocument.cpp
    docks/WorshipDocument.hpp
    docks/WorshipLive.cpp
    docks/WorshipLive.hpp
    docks/WorshipModes.cpp
    docks/WorshipModes.hpp
    docks/WorshipOutput.hpp
    docks/WorshipRemote.cpp
    docks/WorshipRemote.hpp
    docks/WorshipRender.cpp
    docks/WorshipRender.hpp
    docks/WorshipSelfTest.cpp
    docks/WorshipSelfTest.hpp
)

# Phone remote: QR code with the address of the control page.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
find_package(qrcodegencpp REQUIRED)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
target_link_libraries(obs-studio PRIVATE qrcodegencpp::qrcodegencpp)
