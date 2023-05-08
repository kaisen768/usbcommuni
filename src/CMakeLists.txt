aux_source_directory(. source_code)
aux_source_directory(ios source_code)
aux_source_directory(android source_code)

add_library(usbcommuni STATIC ${source_code})

target_link_libraries(usbcommuni 
    cclqueue
    ${IMOBILEDEVICE_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    pthread
    dl
)
