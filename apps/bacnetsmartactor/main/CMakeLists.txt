set(COMPONENT_SRCS "bacnetsmartactor_main.cpp"
"bacnet/ai.c" "bacnet/bip_init.c" "bacnet/bo.c" "bacnet/device.c"
"httpserver/httpserver.cpp")
set(COMPONENT_EMBED_TXTFILES "html/index.html")
set(COMPONENT_EMBED_FILES "html/favicon.ico")
set(COMPONENT_ADD_INCLUDEDIRS "./" "bacnet" "httpserver/include")
register_component()