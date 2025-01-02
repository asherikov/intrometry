APT_INSTALL=sudo apt install -y --no-install-recommends
PIP_INSTALL=sudo python3 -m pip install
GEM_INSTALL=sudo gem install

CLANG_FORMAT?=clang-format15
SCANBUILD?=scan-build-15


help:
	-@grep --color -Ev "(^	)|(^$$)" Makefile
	-@grep --color -Ev "(^	)|(^$$)" GNUmakefile
	-@grep --color -Ev "(^	)|(^$$)" .make/*.mk


install_deps_common:
	${APT_INSTALL} cmake
	${PIP_INSTALL} scspell3k



# static checks
#----------------------------------------------


spell_interactive:
	${MAKE} spell SPELL_XARGS_ARG=-o

# https://github.com/myint/scspell
spell:
	${FIND_SOURCES} \
	    | xargs ${SPELL_XARGS_ARG} scspell --use-builtin-base-dict --override-dictionary ./.utils/qa/scspell.dict


.PHONY: build cmake test
