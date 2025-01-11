FIND_SOURCES=find ./frontend/ ./pjmsg_topic ./pjmsg_mcap/ -iname "*.h" -or -iname "*.cpp" | grep -v 3rdparty

# 0ebaf69 = releases/cpp/v1.4.1
MCAP_SHA=0ebaf69efa2dd5dbe33a882ce51caa8451d518d2
MCAP_DIR=pjmsg_mcap/src/3rdparty/mcap

# 2.2.6
FASTCDR_SHA=9149259a1eb54a9b1104a9f622f89b00a66b6011
FASTCDR_DIR=pjmsg_mcap/src/3rdparty/fastcdr


format:
	${FIND_SOURCES} | xargs ${CLANG_FORMAT} -verbose -i

update_mcap:
	-git remote add mcap https://github.com/foxglove/mcap.git --no-tags
	git fetch --depth=1 mcap ${MCAP_SHA}
	git rm --ignore-unmatch -rf ${MCAP_DIR}
	rm -rf ${MCAP_DIR}
	env GIT_LFS_SKIP_SMUDGE=1 git read-tree --prefix=${MCAP_DIR} -u ${MCAP_SHA}
	find ${MCAP_DIR} \
		-not -wholename "${MCAP_DIR}/cpp/mcap*" \
		-not -wholename "${MCAP_DIR}/cpp" \
		-not -wholename "${MCAP_DIR}" \
		| xargs rm -rf
	cp pjmsg_mcap/src/3rdparty/mcap_visibility.hpp ${MCAP_DIR}/cpp/mcap/include/mcap/visibility.hpp

update_fastcdr:
	-git remote add fastcdr https://github.com/eProsima/Fast-CDR.git --no-tags
	git fetch --depth=1 fastcdr ${FASTCDR_SHA}
	git rm --ignore-unmatch -rf ${FASTCDR_DIR}
	rm -rf ${FASTCDR_DIR}
	env GIT_LFS_SKIP_SMUDGE=1 git read-tree --prefix=${FASTCDR_DIR} -u ${FASTCDR_SHA}
	find ${FASTCDR_DIR} \
		-not -wholename "${FASTCDR_DIR}/cmake/common/check_configuration.cmake" \
		-not -wholename "${FASTCDR_DIR}/cmake/common" \
		-not -wholename "${FASTCDR_DIR}/cmake/packaging*" \
		-not -wholename "${FASTCDR_DIR}/cmake" \
		-not -wholename "${FASTCDR_DIR}/include*" \
		-not -wholename "${FASTCDR_DIR}/src*" \
		-not -wholename "${FASTCDR_DIR}/LICENSE" \
		-not -wholename "${FASTCDR_DIR}" \
		| xargs rm -rf
	cp ${FASTCDR_DIR}/../CMakeLists.txt.fastcdr ${FASTCDR_DIR}/CMakeLists.txt
