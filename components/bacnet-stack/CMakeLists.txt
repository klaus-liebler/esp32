set(COMPONENT_SRCS 	"src/abort.c" "src/address.c" "src/apdu.c" "src/bacaddr.c" "src/bacapp.c" "src/bacdcode.c" "src/bacerror.c" "src/bacint.c" "src/bacreal.c" "src/bacstr.c" "src/bip.c" "src/bvlc.c" "src/cov.c" 
"src/datetime.c" "src/bacdevobjpropref.c" "src/dcc.c" "src/debug.c" "src/iam.c" "src/lighting.c" "src/memcopy.c" "src/npdu.c" "src/proplist.c" "src/reject.c" "src/rp.c" "src/rpm.c" "src/tsm.c" "src/whois.c" "src/wp.c"
"demo/handler/s_iam.c" "demo/handler/noserv.c" "demo/handler/h_cov.c" "demo/handler/h_ucov.c" "demo/handler/h_npdu.c" "demo/handler/h_rp.c" "demo/handler/h_rpm.c" "demo/handler/h_whois.c" "demo/handler/h_wp.c" 
)


set(COMPONENT_ADD_INCLUDEDIRS "include" "ports/lwip" )
set(COMPONENT_PRIV_INCLUDEDIRS "${PROJECT_PATH}/main/bacnet")
set(COMPONENT_REQUIRES lwip driver)
register_component()