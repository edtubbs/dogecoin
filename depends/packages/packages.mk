packages:=boost openssl libevent zeromq
native_packages := native_ccache

qt_packages = qrencode zlib

qt_x86_64_linux_packages:=qt expat libxcb xcb_proto libXau xproto freetype fontconfig libxkbcommon libxcb_util libxcb_util_cursor libxcb_util_render libxcb_util_keysyms libxcb_util_image libxcb_util_wm
qt_i686_linux_packages:=$(qt_x86_64_linux_packages)

qt_darwin_packages=qt
qt_mingw32_packages=qt

ifneq ($(host),$(build))
qt_native_packages := native_qt
endif

wallet_packages=bdb

upnp_packages=miniupnpc

avx2_native_packages:=native_nasm
avx2_x86_64_linux_packages:=intel-ipsec-mb
avx2_x86_64_mingw32_packages:=intel-ipsec-mb

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif
