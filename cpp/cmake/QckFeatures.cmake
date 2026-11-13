# Service availability, not binary-size stripping. Retained adapters still compile.
option(QCK_POLICY_ONLY "Build only the dependency-free policy and its tests" OFF)
option(QCK_ENABLE_OCR "Make OCR services available" ON)
option(QCK_ENABLE_TRANSLATION "Make translation HTTP services available" ON)
option(QCK_ENABLE_CLIPBOARD_HISTORY "Make persistent clipboard history available" ON)
# Opt-in until the inherited administrator/cache and update trust paths are qualified.
option(QCK_ENABLE_ADMIN_STARTUP "Allow administrator task creation and startup" OFF)
option(QCK_ENABLE_UPDATES "Allow extraction and execution of the updater helper" OFF)

foreach(FeatureOption IN ITEMS QCK_POLICY_ONLY QCK_ENABLE_OCR QCK_ENABLE_TRANSLATION
        QCK_ENABLE_CLIPBOARD_HISTORY QCK_ENABLE_ADMIN_STARTUP QCK_ENABLE_UPDATES)
    string(TOUPPER "${${FeatureOption}}" FeatureValue)
    if(NOT FeatureValue MATCHES "^(ON|OFF|TRUE|FALSE|1|0|YES|NO|Y|N)$")
        message(FATAL_ERROR "${FeatureOption} must be an explicit boolean; got '${${FeatureOption}}'")
    endif()
endforeach()
