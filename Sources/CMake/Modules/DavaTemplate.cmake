#
#set( APP_DATA       )

#set( IOS_PLISTT     )
#set( IOS_XIB        )
#
#set( MACOS_PLIST    )
#set( MACOS_ICO      )
#set( MACOS_DYLIB    )
#set( MACOS_XIB      )
#
#set( ANDROID_USE_STANDART_TEMLATE )
#set( ANDROID_DATA_FOLDER          )
#set( ANDROID_PACKAGE              )
#set( ANDROID_APP_NAME             )
#set( ANDROID_ACTIVITY_APP_NAME    )
#set( ANDROID_JAVA_SRC             )
#set( ANDROID_JAVA_LIBS            )
#set( ANDROID_JAVA_RES             )
#set( ANDROID_JAVA_ASSET           ) 
#set( ANDROID_ICO                  )
#
#set( ADDED_SRC                  )
#set( LIBRARIES                  )
#set( LIBRARIES_RELEASE          )
#set( LIBRARIES_DEBUG            )
#set( ADDED_BINARY_DIR           )
#set( EXECUTABLE_FLAG            )
#set( FILE_TREE_CHECK_FOLDERS    )
#
macro( setup_main_executable )

add_definitions ( -D_CRT_SECURE_NO_DEPRECATE )

if( MACOS_DATA )
    set( APP_DATA ${MACOS_DATA} )

elseif( WIN32_DATA )
    set( APP_DATA ${WIN32_DATA} )

elseif( IOS_DATA )
    set( APP_DATA ${IOS_DATA} )

elseif( ANDROID_DATA )
    set( APP_DATA ${ANDROID_DATA} )

endif()

if( ANDROID_USE_STANDART_TEMLATE )
    if( NOT ANDROID_JAVA_SRC )
        list( APPEND ANDROID_JAVA_SRC  ${CMAKE_CURRENT_LIST_DIR}/android/src )    
    endif()

    if( NOT ANDROID_JAVA_RES )
        set( ANDROID_JAVA_RES  ${CMAKE_CURRENT_LIST_DIR}/android/res )    

    endif()

endif()

if( DAVA_FOUND )
    include_directories   ( ${DAVA_INCLUDE_DIR} ) 
    include_directories   ( ${DAVA_THIRD_PARTY_INCLUDES_PATH} )

    list( APPEND ANDROID_JAVA_LIBS  ${DAVA_THIRD_PARTY_ROOT_PATH}/lib_CMake/android/jar )
    list( APPEND ANDROID_JAVA_SRC   ${DAVA_ENGINE_DIR}/Platform/TemplateAndroid/Java )

endif()
 
if( IOS )
    list( APPEND RESOURCES_LIST ${APP_DATA} )    
    list( APPEND RESOURCES_LIST ${IOS_XIB} )
    list( APPEND RESOURCES_LIST ${IOS_PLIST} )
    list( APPEND RESOURCES_LIST ${IOS_ICO} )

elseif( MACOS )
    if( DAVA_FOUND )
        file ( GLOB DYLIB_FILES    ${DAVA_THIRD_PARTY_LIBRARIES_PATH}/*.dylib)
    endif()

    set_source_files_properties( ${DYLIB_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources )

    list ( APPEND DYLIB_FILES     "${DYLIB_FILES}" "${MACOS_DYLIB}" )  

    list( APPEND RESOURCES_LIST  ${APP_DATA}  )
    list( APPEND RESOURCES_LIST  ${DYLIB_FILES} ) 
    list( APPEND RESOURCES_LIST  ${MACOS_XIB}   )    
    list( APPEND RESOURCES_LIST  ${MACOS_PLIST} )
    list( APPEND RESOURCES_LIST  ${MACOS_ICO}   )

    list( APPEND LIBRARIES      ${DYLIB_FILES} )

elseif ( WINDOWS_UAP )

	if(MSVC_VERSION GREATER 1899)
		set(COMPILER_VERSION "14")
	elseif(MSVC_VERSION GREATER 1700)
		set(COMPILER_VERSION "12")
	endif()
	
	set (APP_MANIFEST_NAME Package.appxmanifest)
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsPhone")
		set(PLATFORM WP)
		add_definitions("-DPHONE")
		if("${CMAKE_SYSTEM_VERSION}" STREQUAL "8.0")
			set(APP_MANIFEST_NAME WMAppManifest.xml)
			set(WINDOWS_PHONE8 1)
		endif()
	elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
		set(PLATFORM STORE)
	else()
		set(PLATFORM DESKTOP)
		message(FATAL_ERROR "This app supports Store / Phone only. Please edit the target platform.")
	endif()
	
	set(SHORT_NAME ${PROJECT_NAME})
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
	set(PACKAGE_GUID "6514377e-dfd4-4cdb-80df-4e0366346efc")
	
	if (NOT "${PLATFORM}" STREQUAL "DESKTOP")
		configure_file(
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Manifests/Package_vc${COMPILER_VERSION}.${PLATFORM}.appxmanifest.in
			${CMAKE_CURRENT_BINARY_DIR}/${APP_MANIFEST_NAME}
			@ONLY)
	endif()
	
	if (WINDOWS_PHONE8)
		set(CONTENT_FILES ${CONTENT_FILES}
			${CMAKE_CURRENT_BINARY_DIR}/${APP_MANIFEST_NAME}
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileLarge.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileMedium.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileSmall.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/IconicTileMediumLarge.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/IconicTileSmall.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/ApplicationIcon.png
		)
	  # Windows Phone 8.0 needs to copy all the images.
	  # It doesn't know to use relative paths.
	  file(COPY
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileLarge.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileMedium.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/FlipCycleTileSmall.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/IconicTileMediumLarge.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Tiles/IconicTileSmall.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/ApplicationIcon.png
			DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
		)

	elseif (NOT "${PLATFORM}" STREQUAL "DESKTOP")
	  set(CONTENT_FILES ${CONTENT_FILES}
		${CMAKE_CURRENT_BINARY_DIR}/${APP_MANIFEST_NAME}
		)

		set(ASSET_FILES ${ASSET_FILES}
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/Logo.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/SmallLogo.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/SplashScreen.png
			${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/Assets/StoreLogo.png
		)
	endif()
	
	set(RESOURCE_FILES
		${CONTENT_FILES} ${DEBUG_CONTENT_FILES} ${RELEASE_CONTENT_FILES} ${ASSET_FILES} ${STRING_FILES}
		${DAVA_PLATFORM_SRC}/TemplateWin32/WindowsStore/UnitTests_TemporaryKey.pfx)
	
	list( APPEND RESOURCES_LIST ${RESOURCE_FILES} )

	set_property(SOURCE ${CONTENT_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")
	set_property(SOURCE ${STRING_FILES} PROPERTY VS_TOOL_OVERRIDE "PRIResource")
	set_property(SOURCE ${DEBUG_CONTENT_FILES} PROPERTY VS_DEPLOYMENT_CONTENT $<CONFIG:Debug>)
	set_property(SOURCE ${RELEASE_CONTENT_FILES} PROPERTY
		VS_DEPLOYMENT_CONTENT $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>)

endif()

###

if( QT4_FOUND )
    set( QT_PREFIX "Qt4")	

elseif( QT5_FOUND )
    set( QT_PREFIX "Qt5")	

endif()

if( DAVA_FOUND )
    if( ANDROID )
        include_directories   ( ${DAVA_ENGINE_DIR}/Platform/TemplateAndroid )
        list( APPEND PATTERNS_CPP    ${DAVA_ENGINE_DIR}/Platform/TemplateAndroid/*.cpp )
        list( APPEND PATTERNS_H      ${DAVA_ENGINE_DIR}/Platform/TemplateAndroid/*.h   )        

    endif()

    if( QT_PREFIX )
        if( WIN32 )
            set ( PLATFORM_INCLUDES_DIR ${DAVA_PLATFORM_SRC}/${QT_PREFIX} ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/Win32 )
            list( APPEND PATTERNS_CPP   ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/*.cpp ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/Win32/*.cpp )
            list( APPEND PATTERNS_H     ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/*.h   ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/Win32/*.h   )        

        elseif( MACOS )
            set ( PLATFORM_INCLUDES_DIR  ${DAVA_PLATFORM_SRC}/${QT_PREFIX}  ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/MacOS )
            list( APPEND PATTERNS_CPP    ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/*.cpp ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/MacOS/*.cpp ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/MacOS/*.mm )
            list( APPEND PATTERNS_H      ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/*.h   ${DAVA_PLATFORM_SRC}/${QT_PREFIX}/MacOS/*.h   )        

        endif()
     
        include_directories( ${PLATFORM_INCLUDES_DIR} )

    else()
        if( WIN32 )
            add_definitions        ( -D_UNICODE 
                                     -DUNICODE )
            list( APPEND ADDED_SRC  ${DAVA_PLATFORM_SRC}/TemplateWin32/CorePlatformWin32.cpp 
                                    ${DAVA_PLATFORM_SRC}/TemplateWin32/CorePlatformWin32.h  )

        endif()

    endif()

    file( GLOB_RECURSE CPP_FILES ${PATTERNS_CPP} )
    file( GLOB_RECURSE H_FILES   ${PATTERNS_H} )
    list( APPEND ADDED_SRC ${H_FILES} ${CPP_FILES} )

endif()

###

if( ANDROID )
    add_library( ${PROJECT_NAME} SHARED
        ${ADDED_SRC} 
        ${PROJECT_SOURCE_FILES} 
    )

else()                             
    add_executable( ${PROJECT_NAME} MACOSX_BUNDLE ${EXECUTABLE_FLAG}
        ${ADDED_SRC} 
        ${PROJECT_SOURCE_FILES} 
        ${RESOURCES_LIST}
    )

endif()


if ( QT5_FOUND )
    if ( WIN32 )
        set ( QTCONF_DEPLOY_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/qt.conf" )
    elseif ( APPLE )
        set ( QTCONF_DEPLOY_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PROJECT_NAME}.app/Contents/Resources/qt.conf" )
    endif()

    if     ( TEAMCITY_DEPLOY AND WIN32 )
        set ( PLUGINS_PATH .)
    elseif ( TEAMCITY_DEPLOY AND APPLE )
        set ( PLUGINS_PATH PlugIns )
    else()
        set( PLUGINS_PATH  ${QT5_LIB_PATH}/../plugins )
        get_filename_component( PLUGINS_PATH ${PLUGINS_PATH} ABSOLUTE )
    endif()

    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/QtConfTemplate.in
                    ${CMAKE_CURRENT_BINARY_DIR}/DavaConfigDebug.in  )
    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/QtConfTemplate.in
                    ${CMAKE_CURRENT_BINARY_DIR}/DavaConfigRelWithDebinfo.in  ) 
    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/QtConfTemplate.in
                    ${CMAKE_CURRENT_BINARY_DIR}/DavaConfigRelease.in  )

    ADD_CUSTOM_COMMAND( TARGET ${PROJECT_NAME}  POST_BUILD 
       COMMAND ${CMAKE_COMMAND} -E copy 
       ${CMAKE_CURRENT_BINARY_DIR}/DavaConfig$(CONFIGURATION).in
       ${QTCONF_DEPLOY_PATH}
    )

endif()


if( ANDROID )
    set( LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME}" CACHE PATH "Output directory for Android libs" )

    set( ANDROID_MIN_SDK_VERSION     ${ANDROID_NATIVE_API_LEVEL} )
    set( ANDROID_TARGET_SDK_VERSION  ${ANDROID_TARGET_API_LEVEL} )

    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/AndroidManifest.in
                    ${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml )

    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/AntProperties.in
                    ${CMAKE_CURRENT_BINARY_DIR}/ant.properties )

    if( ANDROID_JAVA_SRC )
        foreach ( ITEM ${ANDROID_JAVA_SRC} )
            execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${ITEM} ${CMAKE_BINARY_DIR}/src )
        endforeach ()
    endif()

    if( ANDROID_JAVA_LIBS )
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${ANDROID_JAVA_LIBS} ${CMAKE_BINARY_DIR}/libs )
    endif()

    if( ANDROID_JAVA_RES )
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${ANDROID_JAVA_RES} ${CMAKE_BINARY_DIR}/res )
    endif()


    if( ANDROID_DATA_FOLDER )
        set( ASSETS_FOLDER "${ANDROID_JAVA_ASSET_FOLDER}" )    

    else()
        get_filename_component( ASSETS_FOLDER ${APP_DATA} NAME )
          
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${APP_DATA} ${CMAKE_BINARY_DIR}/assets/${ASSETS_FOLDER} )

    if( ANDROID_ICO )
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${ANDROID_ICO}  ${CMAKE_BINARY_DIR} )     
    endif()
      
    file ( GLOB SO_FILES ${DAVA_THIRD_PARTY_LIBRARIES_PATH}/*.so )
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME} )
    foreach ( FILE ${SO_FILES} )
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${FILE}  ${CMAKE_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME} )
    endforeach ()

    set_target_properties( ${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION ${DAVA_THIRD_PARTY_LIBRARIES_PATH}/ )

    execute_process( COMMAND ${ANDROID_COMMAND} update project --name ${ANDROID_APP_NAME} --target android-${ANDROID_TARGET_API_LEVEL} --path . )

    if( NOT CMAKE_EXTRA_GENERATOR )
        add_custom_target( ant-configure ALL
            COMMAND  ${ANDROID_COMMAND} update project --name ${ANDROID_APP_NAME} --target android-${ANDROID_TARGET_API_LEVEL} --path .
            COMMAND  ${ANT_COMMAND} release
        )

        add_dependencies( ant-configure ${PROJECT_NAME} )

    endif()


elseif( IOS )
    set_target_properties( ${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${IOS_PLISTT}" 
        RESOURCE                 "${RESOURCES_LIST}"
        XCODE_ATTRIBUTE_INFOPLIST_PREPROCESS YES
    )

    foreach ( TARGET ${PROJECT_NAME} ${DAVA_LIBRARY}  )
        set_xcode_property( ${TARGET} GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Debug] YES )
        set_xcode_property( ${TARGET} ONLY_ACTIVE_ARCH YES )
    endforeach ()

    # Universal (iPad + iPhone)
    set_target_properties( ${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" )

elseif( MACOS )
    set_target_properties ( ${PROJECT_NAME} PROPERTIES
                            MACOSX_BUNDLE_INFO_PLIST "${MACOS_PLIST}" 
                            XCODE_ATTRIBUTE_INFOPLIST_PREPROCESS YES
                            RESOURCE "${RESOURCES_LIST}"
                          )

    if( DEPLOY )
        set( OUTPUT_DIR ${DEPLOY_DIR}/${PROJECT_NAME}.app/Contents )

    else()
        set( OUTPUT_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${PROJECT_NAME}.app/Contents )
    endif()

    set( BINARY_DIR ${OUTPUT_DIR}/MacOS/${PROJECT_NAME} )
    
    if( DAVA_FOUND )
        ADD_CUSTOM_COMMAND(
        TARGET ${PROJECT_NAME}
        POST_BUILD
            COMMAND   
            install_name_tool -change @executable_path/../Frameworks/libfmodex.dylib  @executable_path/../Resources/libfmodex.dylib ${OUTPUT_DIR}/Resources/libfmodevent.dylib  

            COMMAND   
            install_name_tool -change ./libfmodevent.dylib @executable_path/../Resources/libfmodevent.dylib ${BINARY_DIR}    

            COMMAND   
            install_name_tool -change ./libfmodex.dylib @executable_path/../Resources/libfmodex.dylib ${BINARY_DIR}   

            COMMAND 
            install_name_tool -change ./libIMagickHelper.dylib @executable_path/../Resources/libIMagickHelper.dylib ${BINARY_DIR}     

            COMMAND   
            install_name_tool -change ./libTextureConverter.dylib @executable_path/../Resources/libTextureConverter.dylib ${BINARY_DIR}   
        )

    endif()

elseif ( MSVC )       
    if( "${EXECUTABLE_FLAG}" STREQUAL "WIN32" )
        set_target_properties ( ${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ENTRY: /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib" ) 

    else()
        set_target_properties ( ${PROJECT_NAME} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib" )    
    
    endif()


    if( DEBUG_INFO )   
        set_target_properties ( ${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/DEBUG /SUBSYSTEM:WINDOWS" )
    else()
        set_target_properties ( ${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS" )
    endif()

    list( APPEND DAVA_BINARY_WIN32_DIR "${ADDED_BINARY_DIR}" )
    configure_file( ${DAVA_CONFIGURE_FILES_PATH}/DavaVcxprojUserTemplate.in
                    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vcxproj.user @ONLY )

    if( OUTPUT_TO_BUILD_DIR )
        set( OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR} )
        foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
            string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
            set_target_properties ( ${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${OUTPUT_DIR} )
        endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
     endif()
	 
	if ( WINDOWS_UAP )
		set_property(TARGET ${PROJECT_NAME} PROPERTY VS_WINRT_COMPONENT TRUE)
	endif()

endif()

list ( APPEND DAVA_FOLDERS ${DAVA_ENGINE_DIR} )
list ( APPEND DAVA_FOLDERS ${FILE_TREE_CHECK_FOLDERS} )
list ( APPEND DAVA_FOLDERS ${DAVA_THIRD_PARTY_LIBRARIES_PATH} )

file_tree_check( "${DAVA_FOLDERS}" )

if( DAVA_FOUND )
    list ( APPEND LIBRARIES ${DAVA_LIBRARY} )

endif()

if( DAVA_TOOLS_FOUND )
    list ( APPEND LIBRARIES ${DAVA_TOOLS_LIBRARY} )

endif()

target_link_libraries( ${PROJECT_NAME} ${LIBRARIES} )

foreach ( FILE ${LIBRARIES_DEBUG} )
    target_link_libraries  ( ${PROJECT_NAME} debug ${FILE} )
endforeach ()

foreach ( FILE ${LIBRARIES_RELEASE} )
    target_link_libraries  ( ${PROJECT_NAME} optimized ${FILE} )
endforeach ()


###

if( DEPLOY )
   message( "DEPLOY ${PROJECT_NAME} to ${DEPLOY_DIR}")
   execute_process( COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR} )
 
    if( WIN32 )
        if( APP_DATA )
            get_filename_component( DIR_NAME ${APP_DATA} NAME )

            ADD_CUSTOM_COMMAND( TARGET ${PROJECT_NAME}  POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_directory ${APP_DATA}  ${DEPLOY_DIR}/${DIR_NAME}/ 
               COMMAND ${CMAKE_COMMAND} -E remove  ${DEPLOY_DIR}/${PROJECT_NAME}.ilk
            )

        endif()

        foreach ( ITEM fmodex.dll fmod_event.dll IMagickHelper.dll glew32.dll TextureConverter.dll )
            execute_process( COMMAND ${CMAKE_COMMAND} -E copy ${DAVA_TOOLS_BIN_DIR}/${ITEM}  ${DEPLOY_DIR} )
        endforeach ()

        set( OUTPUT_DIR "${DEPLOY_DIR}" )
        foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
            string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
            set_target_properties ( ${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
        endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

    elseif( MACOS )
        set_target_properties( ${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CONFIGURATION_BUILD_DIR  ${DEPLOY_DIR} )

    endif() 

    if( QT_PREFIX )
        qt_deploy( )

    endif()

endif()

endmacro ()

macro( DEPLOY_SCRIPT )

    if( DEPLOY )
        cmake_parse_arguments (ARG "" "" "PYTHON;COPY;COPY_WIN32;COPY_MACOS;COPY_DIR" ${ARGN})

        if( NOT COPY_DIR )
            set( COPY_DIR ${DEPLOY_DIR} )
        endif()

        execute_process( COMMAND ${CMAKE_COMMAND} -E make_directory ${COPY_DIR} )
        execute_process( COMMAND python ${ARG_PYTHON} )

        if( ARG_COPY )
            list( APPEND COPY_LIST ${ARG_COPY} )
        endif()

        if( ARG_COPY_WIN32 AND WIN32 )
            list( APPEND COPY_LIST ${ARG_COPY_WIN32} )
        endif()

        if( ARG_COPY_MACOS AND MACOS )
            list( APPEND COPY_LIST ${ARG_COPY_MACOS} )
        endif()

        foreach ( ITEM ${COPY_LIST} )
            execute_process( COMMAND ${CMAKE_COMMAND} -E copy ${ITEM} ${COPY_DIR} )
        endforeach ()

    endif()
endmacro ()

