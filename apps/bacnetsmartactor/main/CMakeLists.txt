set(COMPONENT_SRCS "bacnetsmartactor_main.cpp"
"ai.c" "bip_init.c" "bo.c" "device.c"
"httpserver/httpserver.cpp"
)
set(COMPONENT_ADD_INCLUDEDIRS "" "httpserver/include")
set(COMPONENT_EMBED_TXTFILES html/index.html)
register_component()