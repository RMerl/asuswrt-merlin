
define dsl_genbintrx_prolog
	@( \
	if [ "$(DSL_TCLINUX)" = "y" ]; then\
		rm -rf image; \
		mkdir image; \
	else \
		echo "call dsl routines"; \
		mkdir reltmp; \
		rm -rf image; \
		mkdir image; \
		rm -rf make_img; \
		rm -rf tc_asus_bin; \
	fi; \
	)
endef

define dsl_genbintrx_epilog
	@( \
	if [ "$(DSL_TCLINUX)" = "y" ]; then \
		echo "Do at build image"; \
	else \
		echo "start to generate DSL , BIN and trx"; \
		cp -R asustools/autobuild-tool/make_img make_img; \
		cp -R asustools/autobuild-tool/tc_asus_bin tc_asus_bin; \
		echo "#!/bin/sh" > cptrx.sh; \
		echo "cp image/DSL*.trx reltmp" >> cptrx.sh; \
		echo "cp image/DSL*.trx make_img" >> cptrx.sh; \
		echo "rm -f image/*.trx" >> cptrx.sh; \
		chmod 777 ./cptrx.sh; \
		./cptrx.sh; \
		rm -f cptrx.sh; \
		cd make_img; \
		echo `find . -maxdepth 1 -name "DSL*.trx"` > tmp.txt; \
		chmod 777 ./LnxBinMrg; \
		./LnxBinMrg `cut -d / -f2 tmp.txt`; \
		echo "#!/bin/sh" > cptrx2.sh; \
		echo "cp DSL*.trx ../image" >> cptrx2.sh; \
		echo "cp DSL*.bin ../image" >> cptrx2.sh; \
		chmod 777 ./cptrx2.sh; \
		./cptrx2.sh; \
		cd ..; \
		rm -rf make_img; \
		rm -rf tc_asus_bin; \
	fi; \
	)
endef

