package=libdogecoin
$(package)_version=0.1.3-dev
$(package)_download_path=https://github.com/dogecoinfoundation/libdogecoin/archive/refs/heads
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=0052ab23e097a24d34739bcea816a9b96c349306077f27fa70d13305a6ef5578
$(package)_dependencies=libevent libunistring

define $(package)_preprocess_cmds
  ./autogen.sh
endef

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --enable-static
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
